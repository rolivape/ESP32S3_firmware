# USB Network Interface AQ (`usb_netif_aq`)

## Overview

This component provides a high-performance USB networking service for ESP32-S3 devices. It implements the USB Network Control Model (NCM) directly on top of the TinyUSB stack, exposing a standard `esp_netif` interface for seamless integration into the ESP-IDF ecosystem.

This component is designed for full control and robustness, avoiding the high-level `esp_tinyusb` wrapper in favor of a direct implementation.

## Features

- **Direct TinyUSB Implementation**: Bypasses `esp_tinyusb` to provide direct control over the USB stack, descriptors, and low-level hardware configuration.
- **`esp_netif` Integration**: Exposes the USB network as a standard `esp_netif` interface, allowing it to work with standard TCP/IP applications (MQTT, HTTP, etc.).
- **Dynamic MAC Address**: Generates a unique, locally-administered MAC address from the chip's eFuse MAC, ensuring no two devices have the same network identity.
- **Custom Descriptors**: Implements a full set of custom USB descriptors in `usb_descriptors_aq.c` to ensure correct enumeration and compatibility with host operating systems (Linux, macOS, Windows). This resolves the common "failed to get mac address" error.
- **Manual PHY Initialization**: Explicitly initializes the ESP32-S3's internal USB PHY, ensuring the hardware is correctly configured before the TinyUSB stack is started.
- **Event-Driven**: Publishes `USB_NET_UP` and `USB_NET_DOWN` events to the `USB_NET_EVENTS` event base.

## API

### `esp_err_t usb_netif_aq_start(void);`

Initializes and starts the USB network interface. This function performs the following key steps:
1.  Generates the dynamic MAC address string for the USB descriptor.
2.  Manually initializes the internal USB PHY.
3.  Initializes the TinyUSB stack (`tusb_init`).
4.  Creates a dedicated FreeRTOS task to run the TinyUSB event handler (`tud_task`).

## Usage Example

The component is started by the `app_manager_aq`. The application can listen for USB network events as follows:

```c
#include "usb_netif_aq.h"
#include "esp_event.h"
#include "esp_log.h"

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == USB_NET_EVENTS) {
        if (event_id == USB_NET_UP) {
            ESP_LOGI("main", "USB network is UP");
            // The network is now ready for use.
        } else if (event_id == USB_NET_DOWN) {
            ESP_LOGI("main", "USB network is DOWN");
        }
    }
}

void app_main(void)
{
    // The app_manager_start() function initializes and starts the USB interface.
    app_manager_start();

    // You can register event handlers to react to network status changes.
    ESP_ERROR_CHECK(esp_event_handler_register(USB_NET_EVENTS, ESP_EVENT_ANY_ID, &event_handler, NULL));
}
```

## Technical Details

The component relies on a set of callbacks from TinyUSB to function:
- `tud_descriptor_..._cb()`: Provide the custom device, configuration, and string descriptors to the host.
- `tud_network_..._cb()`: Handle the NCM network interface initialization, link status changes, and packet reception. These callbacks form the glue layer between TinyUSB and `esp_netif`.
- `tud_mount_cb()` / `tud_umount_cb()`: Log device connection and disconnection events.