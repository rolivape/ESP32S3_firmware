#include "app_manager_aq.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "usb_netif_aq.h"

static const char *TAG = "APP_MANAGER_AQ";

static void app_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == USB_NET_EVENTS) {
        if (event_id == USB_NET_UP) {
            ESP_LOGI(TAG, "USB network is UP");
            esp_netif_t *netif = usb_netif_get_handle();
            if (netif) {
                /**
                 * @brief Start the network stack on this interface.
                 * This action starts the DHCP server (if configured) and allows
                 * TCP/IP traffic to flow.
                 */
                esp_netif_action_start(netif, NULL, 0, NULL);
            }
        } else if (event_id == USB_NET_DOWN) {
            ESP_LOGI(TAG, "USB network is DOWN");
            esp_netif_t *netif = usb_netif_get_handle();
            if (netif) {
                esp_netif_action_stop(netif, NULL, 0, NULL);
            }
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
    ESP_ERROR_CHECK(usb_netif_aq_start());

    ESP_LOGI(TAG, "App Manager started successfully");
}