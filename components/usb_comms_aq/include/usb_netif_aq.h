#ifndef USB_NETIF_AQ_H
#define USB_NETIF_AQ_H

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
 * @brief Starts the USB network interface.
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code.
 */
esp_err_t usb_netif_aq_start(void);

/**
 * @brief Gets the handle of the USB network interface.
 *
 * @return esp_netif_t* The handle of the USB network interface, or NULL if not initialized.
 */
esp_netif_t* usb_netif_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // USB_NETIF_AQ_H