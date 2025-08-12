#pragma once
#include "esp_netif.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t  mac_addr[6];    // Debe coincidir con iMacAddress
    bool     use_ecm_fallback; // true=ECM, false=NCM
    const char *hostname;    // opcional; NULL para omitir
} usb_netif_cfg_aq_t;

esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg);
esp_err_t usb_netif_start_aq(void);     // tinyusb_driver_install + tinyusb_net_init + crear/attach esp_netif
esp_err_t usb_netif_stop_aq(void);
esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out);
bool      usb_netif_is_link_up_aq(void);

// Bloquea hasta GOT_IP o timeout; devuelve IP si se solicita
esp_err_t usb_netif_wait_got_ip_aq(TickType_t timeout, esp_ip4_addr_t *out_ip);
