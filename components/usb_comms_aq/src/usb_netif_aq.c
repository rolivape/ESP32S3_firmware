#include "usb_netif_aq.h"
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
static bool s_network_is_active = false;
uint8_t tud_network_mac_address[6];

// --- Event Base Definition ---
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len);
void netif_driver_free_rx_buffer(void *h, void* buffer);

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

esp_err_t usb_netif_aq_start(void) {
    ESP_LOGI(TAG, "Starting USB NCM network interface...");

    // Create esp_netif object
    if (!s_netif_aq) {
        esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
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
    }

    // Attach driver to esp_netif
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = s_netif_aq,
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_cfg));

    // Initialize TinyUSB driver
    fill_mac_ascii_from_chip();
    ESP_LOGI(TAG, "Initializing TinyUSB stack with custom NCM descriptors");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &tusb_desc_device_aq,
        .string_descriptor = tusb_string_descriptors,
        .configuration_descriptor = tusb_desc_configuration_aq,
        .external_phy = false
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB NCM interface started successfully.");
    return ESP_OK;
}

//--------------------------------------------------------------------+
// esp_netif glue
//--------------------------------------------------------------------+
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
    if (!tud_network_can_xmit(len)) {
        ESP_LOGW(TAG, "Cannot transmit packet, USB backend busy. Dropping packet.");
        return ESP_FAIL;
    }
    tud_network_xmit(buffer, len);
    return ESP_OK;
}

void netif_driver_free_rx_buffer(void *h, void* buffer) {
    free(buffer);
}

//--------------------------------------------------------------------+
// TinyUSB NCM Callbacks
//--------------------------------------------------------------------+
void tud_network_init_cb(void)
{
    ESP_LOGI(TAG, "Network driver initialized");
}

bool tud_network_recv_cb(const uint8_t* data, uint16_t len)
{
    if (!s_netif_aq || !s_network_is_active) return false;

    uint8_t* buffer = malloc(len);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for received packet");
        return false;
    }
    memcpy(buffer, data, len);

    if (esp_netif_receive(s_netif_aq, buffer, len, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_receive failed");
        free(buffer);
        return false;
    }
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = (struct pbuf *)ref;
    struct pbuf *q;
    uint16_t len = 0;

    for(q = p; q != NULL; q = q->next) {
        memcpy(dst, (char *)q->payload, q->len);
        dst += q->len;
        len += q->len;
    }
    
    (void)arg;
    return len;
}

void tud_network_link_state_cb(bool up)
{
    if (up) {
        if (!s_network_is_active) {
            ESP_LOGI(TAG, "USB NCM link is UP. Starting network stack.");
            s_network_is_active = true;
            
            esp_netif_action_start(s_netif_aq, NULL, 0, NULL);
            
            esp_netif_ip_info_t ip_info;
            IP4_ADDR(&ip_info.ip, 192, 168, 7, 1);
            IP4_ADDR(&ip_info.gw, 192, 168, 7, 1);
            IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
            ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif_aq, &ip_info));
            
#if CONFIG_LWIP_DHCPS
            esp_netif_dhcp_option_id_t opt_op = ESP_NETIF_OP_SET;
            esp_netif_dhcp_option_mode_t opt_mode = ESP_NETIF_DOMAIN_NAME_SERVER;
            uint8_t offer = 1;
            ESP_ERROR_CHECK(esp_netif_dhcps_option(s_netif_aq, opt_op, opt_mode, &offer, sizeof(offer)));

            uint32_t lease_time = 120;
            ESP_ERROR_CHECK(esp_netif_dhcps_option(s_netif_aq, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(uint32_t)));

            esp_err_t ret = esp_netif_dhcps_start(s_netif_aq);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "DHCP server started on %s", esp_netif_get_ifkey(s_netif_aq));
            } else {
                ESP_LOGE(TAG, "Failed to start DHCP server: %s", esp_err_to_name(ret));
            }
#endif
            
            esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
        }
    } else {
        if (s_network_is_active) {
            ESP_LOGI(TAG, "USB NCM link is DOWN. Stopping network stack.");
#if CONFIG_LWIP_DHCPS
            ESP_ERROR_CHECK(esp_netif_dhcps_stop(s_netif_aq));
#endif
            esp_netif_action_stop(s_netif_aq, NULL, 0, NULL);
            s_network_is_active = false;
            esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, 0);
        }
    }
}

//--------------------------------------------------------------------+
// TinyUSB Device Callbacks
//--------------------------------------------------------------------+
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
}

void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
}

esp_netif_t* usb_netif_get_handle(void) {
    return s_netif_aq;
}