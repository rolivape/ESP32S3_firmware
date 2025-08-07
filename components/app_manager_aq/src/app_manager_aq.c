#include "app_manager_aq.h"
#include "esp_log.h"
#include "esp_event.h"
#include "usb_comms_aq.h"

static const char *TAG = "APP_MANAGER_AQ";

static void app_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == USB_NET_EVENTS) {
        if (event_id == USB_NET_UP) {
            ESP_LOGI(TAG, "USB network is UP");
            esp_netif_t* netif = usb_comms_get_netif_handle();
            if (netif) {
                esp_netif_ip_info_t ip_info;
                esp_netif_get_ip_info(netif, &ip_info);
                ESP_LOGI(TAG, "IP Address: %d.%d.%d.%d", IP2STR(&ip_info.ip));
            }
        } else if (event_id == USB_NET_DOWN) {
            ESP_LOGI(TAG, "USB network is DOWN");
        }
    }
}

void app_manager_start(void)
{
    ESP_LOGI(TAG, "Starting App Manager");

    // Initialize default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register our custom event handler
    ESP_ERROR_CHECK(esp_event_handler_register(USB_NET_EVENTS, ESP_EVENT_ANY_ID, &app_event_handler, NULL));

    // Start the USB communications service
    ESP_ERROR_CHECK(usb_comms_start());

    ESP_LOGI(TAG, "App Manager started successfully");
}
