/**
 * @file usb_comms_aq.h
 * @brief Public API for the USB Communications AQ service.
 *
 * This component provides a network interface over USB using the NCM protocol.
 * It abstracts the TinyUSB stack and integrates with the ESP-IDF netif layer.
 */

#ifndef USB_COMMS_AQ_H_
#define USB_COMMS_AQ_H_

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

// Declare the event base for USB network events
ESP_EVENT_DECLARE_BASE(USB_NET_EVENTS);

/**
 * @brief Enumeration of USB network events.
 */
typedef enum {
    USB_NET_UP,   /**< The USB network interface is up and running. */
    USB_NET_DOWN, /**< The USB network interface is down. */
} usb_net_event_id_t;

/**
 * @brief Starts the USB communications service.
 *
 * This function initializes the TinyUSB stack, configures the NCM network interface,
 * creates a dedicated task for handling USB communication, and registers the
 * necessary event handlers.
 *
 * @return
 *      - ESP_OK on success.
 *      - ESP_FAIL or other specific error codes on failure.
 */
esp_err_t usb_comms_start(void);

/**
 * @brief Gets the handle of the USB network interface.
 *
 * This function should be called after `usb_comms_start` has successfully completed.
 * The returned handle can be used with other ESP-IDF network functions.
 *
 * @return
 *      - Pointer to the esp_netif_t object if the interface is initialized.
 *      - NULL if the interface is not yet initialized or an error occurred.
 */
esp_netif_t* usb_comms_get_netif_handle(void);

#ifdef __cplusplus
}
#endif

#endif // USB_COMMS_AQ_H_
