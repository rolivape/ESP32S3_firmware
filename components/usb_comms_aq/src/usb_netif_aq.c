#include "usb_netif_aq.h"
#include "usb_descriptors_aq.h"
#include "tusb.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"

// --- Constants and Globals ---
static const char *TAG = "USB_NETIF_AQ";
static esp_netif_t *s_netif_aq = NULL;
static uint8_t s_mac_address[6];

// --- Event Base Definition ---
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
static void tinyusb_task(void *arg);
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len);
static void netif_driver_free_rx_buffer(void *h, void* buffer);

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

esp_err_t usb_netif_aq_start(void) {
    ESP_LOGI(TAG, "Starting USB NCM network interface...");

    // Generate the MAC address string needed for the descriptor
    fill_mac_ascii_from_chip();

    // Initialize the TinyUSB stack
    ESP_LOGI(TAG, "Initializing TinyUSB stack");
    tusb_init();

    // Create a task to handle TinyUSB events
    xTaskCreate(tinyusb_task, "tinyusb_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "USB NCM interface started successfully.");
    return ESP_OK;
}

//--------------------------------------------------------------------+
// TinyUSB Task
//--------------------------------------------------------------------+

static void tinyusb_task(void *arg) {
    while (1) {
        // This is the main processing loop for TinyUSB
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//--------------------------------------------------------------------+
// esp_netif glue
//--------------------------------------------------------------------+

/**
 * @brief Transmit function for the esp_netif driver.
 *
 * This function is called by the TCP/IP stack when it wants to send a packet.
 * It passes the packet to the TinyUSB NCM driver.
 */
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
    // Check if the network interface is ready to transmit
    if (!tud_network_can_xmit(len)) {
        return ESP_FAIL;
    }
    // TinyUSB's NCM driver makes a copy of the buffer, so we don't need to wait.
    tud_network_xmit(buffer, len);
    return ESP_OK;
}

/**
 * @brief Free RX buffer function for the esp_netif driver.
 *
 * This function is called by the TCP/IP stack to free a received buffer
 * after it has been processed.
 */
static void netif_driver_free_rx_buffer(void *h, void* buffer) {
    // The buffer was allocated by TinyUSB, so we use its free function.
    tud_network_recv_renew();
}

//--------------------------------------------------------------------+
// TinyUSB NCM Callbacks
//--------------------------------------------------------------------+

/**
 * @brief Invoked when the NCM network interface is initialized.
 *
 * This is the primary place where we set up the esp_netif instance,
 * as it's only called when the USB host has configured the device.
 */
void tud_network_init_cb(void) {
    ESP_LOGI(TAG, "NCM network interface initialized");

    // --- Create and configure esp_netif ---
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_aq = esp_netif_new(&cfg);
    if (!s_netif_aq) {
        ESP_LOGE(TAG, "Failed to create esp_netif instance");
        return;
    }

    // --- Set up the driver IO functions ---
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = (void *)1, // Non-NULL handle
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_ifconfig));

    // --- Set MAC and IP configuration ---
    ESP_ERROR_CHECK(esp_read_mac(s_mac_address, ESP_MAC_WIFI_STA));
    s_mac_address[0] |= 0x02; // Set as locally administered address
    s_mac_address[5] ^= 0x55;
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, s_mac_address));

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.7.1", &ip_info.ip));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("192.168.7.1", &ip_info.gw));
    ESP_ERROR_CHECK(esp_netif_str_to_ip4("255.255.255.0", &ip_info.netmask));
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_netif_aq));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif_aq, &ip_info));
}

/**
 * @brief Invoked when a network packet is received from the USB host.
 *
 * This function passes the received data to the esp_netif stack for processing.
 */
bool tud_network_recv_cb(const uint8_t *buffer, uint16_t len) {
    if (s_netif_aq) {
        // Pass the received buffer to the TCP/IP stack.
        // esp_netif_receive will make a copy of the data.
        if (esp_netif_receive(s_netif_aq, (void*)buffer, len, NULL) != ESP_OK) {
            ESP_LOGE(TAG, "esp_netif_receive failed");
            return false;
        }
    }
    return true;
}

/**
 * @brief Invoked when the network link state changes (e.g., cable connected/disconnected).
 */
void tud_network_link_state_cb(bool itf_up) {
    if (itf_up) {
        ESP_LOGI(TAG, "NCM network link is UP");
        esp_netif_action_start(s_netif_aq, NULL, 0, NULL);
        esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, portMAX_DELAY);
    } else {
        ESP_LOGI(TAG, "NCM network link is DOWN");
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, portMAX_DELAY);
        esp_netif_action_stop(s_netif_aq, NULL, 0, NULL);
    }
}

//--------------------------------------------------------------------+
// TinyUSB Device Callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
}
