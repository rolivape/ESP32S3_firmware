#include "usb_netif_aq.h"
#include "usb_descriptors_aq.h"
#include "usb_descriptors_internal.h"
#include "tusb.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"
#include "esp_private/usb_phy.h"

// --- Constants and Globals ---
static const char *TAG = "USB_NETIF_AQ";
static esp_netif_t *s_netif_aq = NULL;
static uint8_t s_mac_address[6];
static usb_phy_handle_t s_phy_handle;

ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
static void tinyusb_task(void *arg);
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len);
static void netif_driver_free_rx_buffer(void *h, void* buffer);
static void init_usb_phy(void);

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

esp_err_t usb_netif_aq_start(void) {
    ESP_LOGI(TAG, "Starting USB NCM network interface...");
    fill_mac_ascii_from_chip();
    init_usb_phy();
    ESP_LOGI(TAG, "Initializing TinyUSB stack");
    tusb_init();
    xTaskCreate(tinyusb_task, "tinyusb_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "USB NCM interface started successfully.");
    return ESP_OK;
}

esp_netif_t* usb_netif_get_handle(void) {
    return s_netif_aq;
}

//--------------------------------------------------------------------+
// USB PHY & TinyUSB Task
//--------------------------------------------------------------------+
static void init_usb_phy(void) {
    ESP_LOGI(TAG, "Initializing USB PHY");
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_HIGH,
        .otg_io_conf = NULL,
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_config, &s_phy_handle));
}

static void tinyusb_task(void *arg) {
    while (1) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

//--------------------------------------------------------------------+
// esp_netif glue
//--------------------------------------------------------------------+
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
    if (!tud_network_can_xmit(len)) {
        return ESP_FAIL;
    }
    tud_network_xmit(buffer, len);
    return ESP_OK;
}

static void netif_driver_free_rx_buffer(void *h, void* buffer) {
    tud_network_recv_renew();
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when the host selects an alternate setting for an interface
bool tud_set_interface_cb(uint8_t itf, uint8_t alt)
{
    // Only handle the Data Class interface
    if (itf != ITF_NUM_CDC_NCM_DATA) {
        return true;
    }

    if (alt == 1) {
        ESP_LOGI(TAG, "Host selected alternate setting 1 for NCM Data interface.");
        // Create the esp_netif instance now that the data endpoints are active
        if (!s_netif_aq) {
            const esp_netif_ip_info_t ip_info = {
                .ip = { .addr = ESP_IP4TOADDR( 192, 168, 7, 1) },
                .gw = { .addr = ESP_IP4TOADDR( 192, 168, 7, 1) },
                .netmask = { .addr = ESP_IP4TOADDR( 255, 255, 255, 0) },
            };
            const esp_netif_inherent_config_t inherent_cfg = {
                .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
                .ip_info = &ip_info,
                .if_key = "USB_NCM_AQ",
                .if_desc = "AquaController USB NCM",
                .route_prio = 50
            };
            esp_netif_config_t cfg = { .base = &inherent_cfg, .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH };
            s_netif_aq = esp_netif_new(&cfg);
            if (!s_netif_aq) {
                ESP_LOGE(TAG, "Failed to create esp_netif instance");
                return false;
            }
            const esp_netif_driver_ifconfig_t driver_ifconfig = {
                .handle = (void *)1,
                .transmit = netif_driver_transmit_aq,
                .driver_free_rx_buffer = netif_driver_free_rx_buffer
            };
            ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_ifconfig));
            ESP_ERROR_CHECK(esp_read_mac(s_mac_address, ESP_MAC_WIFI_STA));
            s_mac_address[0] |= 0x02;
            s_mac_address[5] ^= 0x55;
            ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, s_mac_address));
        }
    } else {
        ESP_LOGW(TAG, "Host selected alternate setting %d. Network interface will not be active.", alt);
    }
    return true;
}

bool tud_network_recv_cb(const uint8_t *buffer, uint16_t len) {
    if (s_netif_aq && esp_netif_receive(s_netif_aq, (void*)buffer, len, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_receive failed");
        return false;
    }
    return true;
}

void tud_network_link_state_cb(bool itf_up) {
    if (itf_up) {
        ESP_LOGI(TAG, "NCM network link is UP");
        esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, portMAX_DELAY);
    } else {
        ESP_LOGI(TAG, "NCM network link is DOWN");
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, portMAX_DELAY);
    }
}