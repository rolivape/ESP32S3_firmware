#include "usb_ppp_netif_aq.h"
#include "usb_descriptors_aq.h"
#include "tinyusb.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "lwip/ip_addr.h"

static const char *TAG = "USB_PPP_NETIF_AQ";

ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

static esp_netif_t *s_ppp_netif = NULL;

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_id == NETIF_PPP_ERRORUSER) {
        esp_netif_t *netif = *(esp_netif_t **)event_data;
        ESP_LOGI(TAG, "PPP Link is down on netif %p", netif);
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, 0);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_netif_t *netif = event->esp_netif;
    
    ESP_LOGI(TAG, "PPP Link is up on netif %p", netif);
    ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));

    // Start DHCP server on the PPP interface
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));
    ESP_LOGI(TAG, "DHCP server started on PPP netif");

    esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
}

static esp_err_t ppp_output_callback(void *ctx, void *buffer, size_t len)
{
    tud_cdc_write(buffer, len);
    tud_cdc_write_flush();
    return ESP_OK;
}

void tud_cdc_rx_cb(uint8_t itf)
{
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
    uint32_t count = tud_cdc_read(buf, sizeof(buf));
    if (s_ppp_netif) {
        esp_netif_receive(s_ppp_netif, buf, count, NULL);
    }
}

esp_err_t usb_ppp_netif_aq_start(void)
{
    ESP_LOGI(TAG, "Starting USB PPP network interface...");

    // 1. Initialize TinyUSB with CDC-ACM descriptors
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &tusb_desc_device_aq,
        .string_descriptor = tusb_string_descriptors,
        .configuration_descriptor = tusb_desc_configuration_aq,
        .external_phy = false
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // 2. Create PPP netif
    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = (void *)1,
        .transmit = ppp_output_callback,
        .driver_free_rx_buffer = NULL
    };
    
    esp_netif_inherent_config_t base_netif_config = ESP_NETIF_INHERENT_DEFAULT_PPP();
    esp_netif_config_t netif_ppp_config = { .base = &base_netif_config, .driver = &driver_ifconfig, .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP };
    s_ppp_netif = esp_netif_new(&netif_ppp_config);
    assert(s_ppp_netif);

    // 3. Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    // 4. Set static IP for the PPP server side
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 7, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 7, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_ppp_netif, &ip_info));

    ESP_LOGI(TAG, "USB PPP interface started successfully.");
    return ESP_OK;
}
