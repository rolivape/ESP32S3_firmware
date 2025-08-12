#include "usb_comms_aq.h"
#include "usb_netif_aq.h"
#include "esp_log.h"

static const char *TAG = "usb_comms_aq";

esp_err_t usb_comms_init_aq(void)
{
    ESP_LOGI(TAG, "Initializing USB communications");
    usb_netif_cfg_aq_t cfg = {
        .use_ecm_fallback = false,
        .hostname = "esp32-usbncm",
    };
    ESP_ERROR_CHECK(usb_netif_install_aq(&cfg));
    return usb_netif_start_aq();
}

esp_err_t usb_comms_wait_link_aq(TickType_t timeout, esp_ip4_addr_t *out_ip)
{
    ESP_LOGI(TAG, "Waiting for IP address...");
    return usb_netif_wait_got_ip_aq(timeout, out_ip);
}

esp_err_t usb_comms_stop_aq(void)
{
    ESP_LOGI(TAG, "Stopping USB communications");
    return usb_netif_stop_aq();
}
