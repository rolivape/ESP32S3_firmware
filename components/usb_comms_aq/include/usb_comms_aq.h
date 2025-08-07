/**
 * @file usb_comms_aq.h
 * @brief Public API for the USB Communications AQ service.
 *
 * This component provides a network interface over USB using the NCM protocol.
 * It abstracts the TinyUSB stack and integrates with the ESP-IDF netif layer.
 */

#ifndef USB_COMMS_AQ_H
#define USB_COMMS_AQ_H

#include "esp_netif.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "tusb.h" // Required for descriptor types

// --- Custom USB Descriptors ---
extern tusb_desc_device_t const desc_device_aq;
extern uint8_t const desc_configuration_aq[];
extern char const* string_desc_arr_aq[];

// Event base for USB network events
ESP_EVENT_DECLARE_BASE(USB_NET_EVENTS);


/**
 * @brief Enum for USB network events
 */
enum {
    USB_NET_UP,
    USB_NET_DOWN,
};

/**
 * @brief Starts the USB Communications AQ service.
 *
 * This function initializes the USB NCM interface, sets up the esp_netif layer,
 * and starts the necessary tasks to handle USB communication.
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code.
 */
esp_err_t usb_comms_start(void);

/**
 * @brief Gets the handle for the USB network interface.
 *
 * @return esp_netif_t* Handle to the esp_netif instance, or NULL if not initialized.
 */
esp_netif_t* usb_comms_get_netif_handle(void);

#endif // USB_COMMS_AQ_H
