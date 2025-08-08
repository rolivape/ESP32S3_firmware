/**
 * @file usb_netif_aq.h
 * @brief Public API for the USB NCM Network Interface.
 *
 * This component provides a network interface over USB using the NCM protocol,
 * implemented directly on the TinyUSB stack. It integrates with the ESP-IDF
 * netif layer to provide a standard network interface to the system.
 */

#ifndef USB_NETIF_AQ_H
#define USB_NETIF_AQ_H

#include "esp_netif.h"
#include "esp_event.h"

// Event base for USB network status events
ESP_EVENT_DECLARE_BASE(USB_NET_EVENTS);

/**
 * @brief Enum for USB network events dispatched to the default event loop.
 */
enum {
    USB_NET_UP,   /**< The USB network interface is connected and ready. */
    USB_NET_DOWN, /**< The USB network interface is disconnected. */
};

/**
 * @brief Starts the USB NCM network interface.
 *
 * This function performs the following steps:
 * 1. Generates the dynamic MAC address string required for the USB descriptor.
 * 2. Initializes the underlying TinyUSB stack (`tusb_init`).
 * 3. Creates a FreeRTOS task to handle the TinyUSB event processing loop (`tud_task`).
 *
 * Once this function returns, the USB device is ready to be connected to a host.
 * The actual network interface setup (`esp_netif`) occurs in callbacks when the
 * host enables the NCM interface.
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code.
 */
esp_err_t usb_netif_aq_start(void);

#endif // USB_NETIF_AQ_H