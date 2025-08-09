#include "usb_netif_aq.h"
#include "usb_ncm_cb_aq.h"
#include "usb_descriptors_aq.h"
#include "tinyusb.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

// --- Constants and Globals ---
static const char *TAG = "USB_NETIF_AQ";
esp_netif_t *s_netif_aq = NULL;
uint8_t s_mac_address[6] __attribute__((used));
static bool s_is_netif_up = false;

// --- Event Base Definition ---
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) __attribute__((used));
void netif_driver_free_rx_buffer(void *h, void* buffer) __attribute__((used));

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

esp_err_t usb_netif_aq_start(void) {
    ESP_LOGI(TAG, "Starting USB NCM network interface...");

    // Step 1: Create esp_netif object if not already created
    if (!s_netif_aq) {
        esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
        // Disable DHCP client by default, as we will run a DHCP server
        base_cfg.flags &= ~ESP_NETIF_DHCP_CLIENT;
        base_cfg.if_desc = "usb-ncm";
        base_cfg.route_prio = 30;

        esp_netif_config_t cfg = {
            .base = &base_cfg,
            .driver = NULL,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
        };

        s_netif_aq = esp_netif_new(&cfg);
        if (!s_netif_aq) {
            ESP_LOGE(TAG, "Failed to create esp_netif for USB NCM");
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "Step 1: esp_netif object created");
    }

    // Step 2: Attach driver to esp_netif
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = s_netif_aq,
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_cfg));
    ESP_LOGD(TAG, "Step 2: Driver attached to esp_netif");

    // Step 3: Initialize TinyUSB driver
    fill_mac_ascii_from_chip();
    ESP_LOGI(TAG, "Initializing TinyUSB stack with custom descriptors");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &tusb_desc_device_aq,
        .string_descriptor = tusb_string_descriptors,
        .configuration_descriptor = tusb_desc_configuration_aq,
        .external_phy = false
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGD(TAG, "Step 3: TinyUSB driver installed");

    ESP_LOGI(TAG, "USB NCM interface started successfully.");
    return ESP_OK;
}

//--------------------------------------------------------------------+
// esp_netif glue
//--------------------------------------------------------------------+
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
    if (!tud_network_can_xmit(len)) {
        return ESP_FAIL;
    }
    tud_network_xmit(buffer, len);
    return ESP_OK;
}

void netif_driver_free_rx_buffer(void *h, void* buffer) {
    tud_network_recv_renew();
}

//--------------------------------------------------------------------+
// TinyUSB NCM Callbacks
//--------------------------------------------------------------------+
bool tud_network_recv_cb(const uint8_t* data, uint16_t len)
{
    if (!s_netif_aq) return false;
    esp_netif_receive(s_netif_aq, (void*)data, len, NULL);
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    (void)dst;
    (void)ref;
    (void)arg;
    return 0;
}

//--------------------------------------------------------------------+
// TinyUSB Device Callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
    if (!s_is_netif_up) {
        // Start the network interface
        esp_netif_action_start(s_netif_aq, NULL, 0, NULL);
        ESP_LOGD(TAG, "Network interface up");

        // Set static IP address
        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 7, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 7, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif_aq, &ip_info));
        ESP_LOGD(TAG, "Static IP configured");

        // Start DHCP server
        ESP_ERROR_CHECK(esp_netif_dhcps_start(s_netif_aq));
        ESP_LOGI(TAG, "DHCP server started on %s", esp_netif_get_ifkey(s_netif_aq));
        
        s_is_netif_up = true;
    }
}

void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
    if (s_is_netif_up) {
        esp_netif_action_stop(s_netif_aq, NULL, 0, NULL);
        s_is_netif_up = false;
    }
}

bool tud_set_interface_cb(uint8_t itf, uint8_t alt)
{
    if (itf == 1 && alt == 1) { // alt-setting 1 = link active
        ESP_LOGI(TAG, "USB NCM interface is UP.");
        esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
    } else if (itf == 1 && alt == 0) { // alt-setting 0 = link down
        ESP_LOGI(TAG, "USB NCM interface is DOWN.");
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, 0);
    }
    return true;
}

esp_netif_t* usb_netif_get_handle(void) {
    return s_netif_aq;
}