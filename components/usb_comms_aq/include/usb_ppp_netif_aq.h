#ifndef USB_PPP_NETIF_AQ_H
#define USB_PPP_NETIF_AQ_H

#include "esp_netif.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(USB_NET_EVENTS);

typedef enum {
    USB_NET_UP,
    USB_NET_DOWN,
} usb_net_event_t;

/**
 * @brief Starts the USB PPP network interface.
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code.
 */
esp_err_t usb_ppp_netif_aq_start(void);

#ifdef __cplusplus
}
#endif

#endif // USB_PPP_NETIF_AQ_H
