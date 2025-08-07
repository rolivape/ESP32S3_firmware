/**
 * @file usb_comms_aq.c
 * @brief Core implementation of the USB Communications AQ service.
 */

#include "usb_comms_aq.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "tinyusb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"
#include "class/net/ncm.h"

// --- Constants and Globals ---
static const char *TAG = "USB_COMMS_AQ";
#define TX_QUEUE_SIZE 10
#define TASK_STACK_SIZE (4096)
#define TASK_PRIORITY 5

// Event base definition
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// Static handles
static esp_netif_t *s_netif_aq = NULL;
static QueueHandle_t s_tx_queue = NULL;
static bool s_is_up = false;

// Metrics
static uint32_t s_rx_packets = 0;
static uint32_t s_tx_packets = 0;
static uint32_t s_tx_drops = 0;
static uint32_t s_tx_retries = 0;

// --- Forward Declarations ---
static void usb_comms_task(void *pvParameters);
static esp_err_t netif_transmit_aq(void *h, void *buffer, size_t len);

// --- Public API ---

esp_netif_t* usb_comms_get_netif_handle(void) {
    return s_netif_aq;
}

esp_err_t usb_comms_start(void) {
    ESP_LOGI(TAG, "Starting USB Comms AQ service...");

    // Create the TX queue
    s_tx_queue = xQueueCreate(TX_QUEUE_SIZE, sizeof(struct pbuf *));
    if (s_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        return ESP_FAIL;
    }

    // --- Create and configure esp_netif ---
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_aq = esp_netif_new(&cfg);
    if (s_netif_aq == NULL) {
        ESP_LOGE(TAG, "Failed to create esp_netif instance");
        vQueueDelete(s_tx_queue);
        return ESP_FAIL;
    }

    // IO driver configuration
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = (void *)1, // Can be any non-null pointer
        .transmit = netif_transmit_aq,
        .driver_free_rx_buffer = NULL // We copy data, so LWIP can free its own pbufs
    };

    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_ifconfig));

    // Set static IP configuration
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.7.1", &ip_info.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.7.1", &ip_info.gw));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("255.255.255.0", &ip_info.netmask));
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_netif_aq));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif_aq, &ip_info));

    // Set MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    mac[0] |= 0x02; // Set local bit
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, mac));

    // --- Initialize TinyUSB ---
    ESP_LOGI(TAG, "Initializing TinyUSB stack...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL, // Using default NCM descriptor
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Create the dedicated task for USB communications
    xTaskCreatePinnedToCore(usb_comms_task, "usb_comms_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL, 0);

    ESP_LOGI(TAG, "USB Comms AQ service started successfully");
    return ESP_OK;
}

// --- esp_netif IO Driver ---

/**
 * @brief Transmit function for esp_netif.
 *
 * This function is called by the TCP/IP stack when it wants to send a packet.
 * It queues the packet to be sent by the dedicated USB task.
 * This function is in a hot-path but IRAM_ATTR is not applied as per instructions
 * to only apply it to the most critical callbacks.
 */
static esp_err_t netif_transmit_aq(void *h, void *buffer, size_t len) {
    if (!s_is_up) {
        return ESP_FAIL;
    }

    struct pbuf *p = (struct pbuf *)buffer;
    // Create a reference to the pbuf to be sent.
    // The pbuf will be freed once it's transmitted (in tud_network_xmit_cb_aq).
    pbuf_ref(p);

    if (xQueueSend(s_tx_queue, &p, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue full, dropping packet");
        pbuf_free(p); // Free the reference
        s_tx_drops++;
        return ESP_FAIL;
    }

    return ESP_OK;
}

// --- TinyUSB Callbacks ---

/**
 * @brief Invoked when USB device is mounted.
 * IRAM_ATTR is applied as this is a critical, time-sensitive callback.
 */
void IRAM_ATTR tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
    s_is_up = true;
    // Notify the system that the network is up
    esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
    esp_netif_action_start(s_netif_aq, NULL, 0, NULL);
}

/**
 * @brief Invoked when USB device is unmounted.
 * IRAM_ATTR is applied as this is a critical, time-sensitive callback.
 */
void IRAM_ATTR tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
    s_is_up = false;
    // Notify the system that the network is down
    esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, 0);
    esp_netif_action_stop(s_netif_aq, NULL, 0, NULL);
}

/**
 * @brief Invoked when network data is received from the host.
 * This is a hot-path and is marked with IRAM_ATTR.
 */
bool IRAM_ATTR tud_network_recv_cb(const uint8_t *data, uint16_t size) {
    if (!s_netif_aq) return false;

    // Allocate a pbuf and copy the received data
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p) {
        memcpy(p->payload, data, size);
        // Pass the packet to the TCP/IP stack
        if (esp_netif_receive(s_netif_aq, p->payload, p->len, p) != ESP_OK) {
            pbuf_free(p);
        } else {
            s_rx_packets++;
        }
    }
    return true;
}

/**
 * @brief Invoked when a packet has been successfully transmitted.
 * This is a hot-path and is marked with IRAM_ATTR.
 */
uint16_t IRAM_ATTR tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
    struct pbuf *p = (struct pbuf *)ref;
    memcpy(dst, p->payload, p->len);
    pbuf_free(p); // Free the pbuf that was transmitted
    s_tx_packets++;
    return p->len;
}

// --- Dedicated USB Task ---

/**
 * @brief Task to handle asynchronous packet transmission.
 *
 * This task waits for packets to appear in the TX queue and sends them
 * over USB using a non-blocking, asynchronous call.
 */
static void usb_comms_task(void *pvParameters) {
    struct pbuf *p = NULL;

    for (;;) {
        // Wait for a packet to be queued
        if (xQueueReceive(s_tx_queue, &p, portMAX_DELAY) == pdTRUE) {
            // Wait until the USB host is ready to receive data
            while (!tud_network_can_xmit(p->tot_len)) {
                s_tx_retries++;
                vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to prevent busy-waiting
            }

            // tud_network_xmit expects a pbuf chain
            tud_network_xmit(p, 0);
        }
    }
    vTaskDelete(NULL);
}
