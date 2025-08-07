# USB Communications AQ Component (`usb_comms_aq`)

## Overview

This component provides a high-performance USB networking service for ESP32-S3 devices, abstracting the TinyUSB stack to expose a standard `esp_netif` interface. It is designed for modularity and performance, focusing on the USB Network Control Model (NCM) for maximum compatibility and throughput with modern operating systems (Windows 10+, macOS, Linux).

## Features

- **`esp_netif` Integration**: Exposes USB networking as a standard network interface, allowing seamless integration with other network services like MQTT, HTTP, etc.
- **High-Performance**: Utilizes asynchronous, zero-copy transmission and a dedicated task to manage data flow, optimized for high throughput.
- **NCM Protocol**: Uses the modern NCM protocol for efficient packet aggregation.
- **Event-Driven**: Publishes `USB_NET_UP` and `USB_NET_DOWN` events to the `USB_NET_EVENTS` event base, allowing other application components to react to changes in USB connection state.
- **Static IP**: Configured with a default static IP address (`192.168.7.1`) for predictable network access.
- **Optimized**: Critical callbacks are placed in IRAM to reduce latency.

### MAC Address and USB Descriptors

To ensure stable and correct USB enumeration, this component uses a full set of custom USB descriptors defined in `src/tusb_descriptors_aq.c`. This approach provides explicit control over the device's identity and capabilities, resolving potential issues with host OS compatibility.

- **Device Descriptor**: Identifies the device with a unique Vendor ID (Espressif) and a custom Product ID.
- **Configuration Descriptor**: Defines the device as a CDC-NCM network interface.
- **String Descriptors**: Provides human-readable strings for the Manufacturer, Product, and a unique Serial Number derived from the device's eFuse MAC address.

This manual configuration ensures that the MAC address is correctly exposed to the host, fixing the "failed to get mac address" error and guaranteeing a reliable network connection.

## API

### `esp_err_t usb_comms_start(void);`

Initializes and starts the USB communications service. This function sets up the TinyUSB stack, creates the `esp_netif` instance, and starts the dedicated communication task.

### `esp_netif_t* usb_comms_get_netif_handle(void);`

Retrieves a handle to the underlying `esp_netif_t` instance. This handle can be used with other ESP-IDF networking APIs. Returns `NULL` if the service has not been started.

## Configuration

The component can be enabled or disabled via `menuconfig`:

`Component config` -> `USB Comms AQ Configuration` -> `[*] Enable USB Comms AQ Service`

## Usage Example

```c
#include "usb_comms_aq.h"
#include "esp_event.h"
#include "esp_log.h"

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == USB_NET_EVENTS) {
        if (event_id == USB_NET_UP) {
            ESP_LOGI("main", "USB network is UP");
            esp_netif_t* netif = usb_comms_get_netif_handle();
            // You can now use the netif handle
        } else if (event_id == USB_NET_DOWN) {
            ESP_LOGI("main", "USB network is DOWN");
        }
    }
}

void app_main(void)
{
    // Initialize event loop and register handler for USB events
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(USB_NET_EVENTS, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // Start the USB communications service
    ESP_ERROR_CHECK(usb_comms_start());
}
```

## Performance

- **Expected Throughput**: 1-6 Mbps, dependent on host OS and system load.
- **IRAM Usage**: `IRAM_ATTR` is surgically applied to performance-critical TinyUSB callbacks (`tud_mount_cb`, `tud_umount_cb`, `tud_network_recv_cb`, `tud_network_xmit_cb`) to minimize latency without excessive IRAM consumption. Use `idf.py size` to analyze the impact.

## Testing

Integration testing can be performed using `iperf3` to measure network throughput between the ESP32-S3 and a connected host computer. Wireshark can be used to inspect the NCM traffic.
