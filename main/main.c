#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_manager_aq.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Calling app_main()");
    app_manager_start();

    // app_main should not return
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}