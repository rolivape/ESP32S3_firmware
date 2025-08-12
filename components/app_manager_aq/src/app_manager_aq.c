#include "app_manager_aq.h"
#include "esp_log.h"
#include "esp_event.h"
#include "usb_comms_aq.h"

static const char *TAG = "app_manager_aq";

void app_manager_start(void)
{
    ESP_LOGI(TAG, "Starting App Manager");

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(usb_comms_init_aq());

    esp_ip4_addr_t ip = {0};
    esp_err_t w = usb_comms_wait_link_aq(pdMS_TO_TICKS(CONFIG_AQ_COMMS_WAIT_MS), &ip);
    if (w == ESP_OK) {
        ESP_LOGI(TAG, "USB link ready, starting MQTT");
        // ESP_ERROR_CHECK(mqtt_service_start_aq()); // Uncomment when mqtt_service_aq is available
    } else {
        ESP_LOGE(TAG, "USB link timeout, no IP");
    }
}
