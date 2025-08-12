#pragma once
#include "esp_netif.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>

esp_err_t usb_comms_init_aq(void);
esp_err_t usb_comms_wait_link_aq(TickType_t timeout, esp_ip4_addr_t *out_ip);
esp_err_t usb_comms_stop_aq(void);
