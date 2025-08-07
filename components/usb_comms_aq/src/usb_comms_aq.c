/**
 * @file usb_comms_aq.c
 * @brief Core implementation of the USB Communications AQ service.
 */

#include "usb_comms_aq.h"
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_efuse.h"
#include "esp_netif.h"
#include "tinyusb.h"
#include "tinyusb_net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"
#include "class/net/ncm.h"

// --- Constants and Globals ---
static const char *TAG = "USB_COMMS_AQ";
#define TASK_STACK_SIZE (4096)
#define TASK_PRIORITY 5

// Event base definition
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// Static handles
static esp_netif_t *s_netif_aq = NULL;
static bool s_is_up = false;

// Metrics
static uint32_t s_rx_packets = 0;
static uint32_t s_tx_packets = 0;

// --- Forward Declarations ---
static esp_err_t netif_transmit_aq(void *h, void *buffer, size_t len);
static esp_err_t usb_net_rx_callback(void *buffer, uint16_t len, void *ctx);

// --- Public API ---

esp_netif_t* usb_comms_get_netif_handle(void) {
    return s_netif_aq;
}

esp_err_t usb_comms_start(void) {
    ESP_LOGI(TAG, "Starting USB Comms AQ service...");

    // --- Create and configure esp_netif ---
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_aq = esp_netif_new(&cfg);
    if (s_netif_aq == NULL) {
        ESP_LOGE(TAG, "Failed to create esp_netif instance");
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

    // --- Initialize TinyUSB ---
    ESP_LOGI(TAG, "Initializing TinyUSB stack...");
    
    // --- MAC Address Configuration ---
    // Per design, generate a locally administered MAC address from the base eFuse MAC.
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get base MAC address from eFuse, using static MAC. Error: %s", esp_err_to_name(err));
        // Fallback to a static MAC address as per resilience requirements
        const uint8_t static_mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
        memcpy(mac, static_mac, sizeof(mac));
    } else {
        // Set as a locally administered address (second-least significant bit of the first octet)
        mac[0] |= 0x02;
    }

    ESP_LOGI(TAG, "Generated MAC: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Update the MAC address string descriptor
    static char mac_str[18];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    string_desc_arr_aq[4] = mac_str;

    // Set MAC address for the netif
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, mac));

    tinyusb_net_config_t net_config = {
        .user_context = s_netif_aq,
        .on_recv_callback = usb_net_rx_callback,
    };
    memcpy(net_config.mac_addr, mac, sizeof(net_config.mac_addr));

    ESP_LOGI(TAG, "Initializing TinyUSB with custom descriptors...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &desc_device_aq,
        .string_descriptor = string_desc_arr_aq,
        .external_phy = false,
        .configuration_descriptor = desc_configuration_aq,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB Comms AQ service started successfully");
    return ESP_OK;
}

// --- esp_netif IO Driver ---

/**
 * @brief Transmit function for esp_netif.
 */
static esp_err_t netif_transmit_aq(void *h, void *buffer, size_t len) {
    if (!s_is_up) {
        return ESP_FAIL;
    }

    if (tinyusb_net_send_sync(buffer, len, NULL, portMAX_DELAY) == ESP_OK) {
        s_tx_packets++;
        return ESP_OK;
    }

    return ESP_FAIL;
}

// --- TinyUSB Callbacks ---

/**
 * @brief Invoked when USB device is mounted.
 */
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
    s_is_up = true;
    // Notify the system that the network is up
    esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
    esp_netif_action_start(s_netif_aq, NULL, 0, NULL);
}

/**
 * @brief Invoked when USB device is unmounted.
 */
void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
    s_is_up = false;
    // Notify the system that the network is down
    esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, 0);
    esp_netif_action_stop(s_netif_aq, NULL, 0, NULL);
}

/**
 * @brief Wrapper for receiving network data from TinyUSB.
 */
static esp_err_t usb_net_rx_callback(void *buffer, uint16_t len, void *ctx)
{
    esp_netif_t *netif = (esp_netif_t *)ctx;
    if (!netif) {
        return ESP_FAIL;
    }

    // Pass the packet to the TCP/IP stack
    if (esp_netif_receive(netif, buffer, len, NULL) != ESP_OK) {
        return ESP_FAIL;
    } else {
        s_rx_packets++;
    }
    return ESP_OK;
}
