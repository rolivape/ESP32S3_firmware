# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-S3 firmware project that implements USB network connectivity using NCM (Network Control Model) over USB. The ESP32-S3 acts as a USB device providing network interface to a host (typically Raspberry Pi) which serves as the DHCP server.

## Build Commands

```bash
# Configure and build the project
idf.py build

# Flash to device
idf.py flash

# Monitor serial output
idf.py monitor

# Clean build artifacts
idf.py fullclean

# Flash and monitor in one command
idf.py flash monitor
```

## Project Architecture

### Component Hierarchy
- **main/** - Entry point, calls app_manager_start()
- **app_manager_aq/** - Application orchestrator, coordinates initialization sequence
- **usb_comms_aq/** - High-level USB communications wrapper
- **usb_netif_aq/** - Core USB network interface implementation using TinyUSB
- **usb_com_ms_aq/** - Legacy component (may be removed)

### Network Stack
```
Host (RPi) <--USB NCM/ECM--> ESP32-S3
     ^                           ^
DHCP Server                 DHCP Client
```

### Initialization Flow
```
main() → app_manager_start() → usb_comms_init_aq() → usb_netif_start_aq()
                            ↓
                     Wait for IP (configurable timeout)
                            ↓
                     Start MQTT service (when available)
```

## Key Design Principles

### Naming Convention
- All custom components, files, and global functions use `_aq` suffix
- Examples: `usb_comms_aq`, `app_manager_aq.h`, `usb_netif_start_aq()`

### Error Handling
- Functions return `esp_err_t`
- Always check return values from ESP-IDF functions
- Use `ESP_ERROR_CHECK()` for critical operations

### Memory Management
- Code runs from Flash by default
- Use `IRAM_ATTR` only for hot-path callbacks (TinyUSB callbacks)
- Protect shared resources with FreeRTOS primitives

### Network Architecture
- ESP32-S3 is DHCP **client** (not server)
- Host (RPi) provides DHCP server functionality
- USB interface uses NCM (preferred) with ECM fallback

## Important Files

### Configuration
- `sdkconfig.defaults` - Default build configuration
- `components/*/Kconfig` - Component-specific configuration options
- `CONFIG_AQ_COMMS_WAIT_MS` - Timeout for USB link establishment (default: 8000ms)

### Core Implementation
- `components/usb_netif_aq/src/usb_netif_aq.c` - Main USB network implementation
- `components/usb_comms_aq/src/usb_comms_aq.c` - High-level communications API
- `components/app_manager_aq/src/app_manager_aq.c` - Application orchestration

### USB Descriptors
- `components/usb_netif_aq/src/usb_descriptors_aq.c` - USB device descriptors
- MAC address is auto-generated from ESP32-S3 efuse with local admin bit set

## Development Notes

### Current DHCP Implementation Conflict
There's an architectural inconsistency between documentation (which specifies DHCP client) and some code (which implements DHCP server). The correct implementation per documentation is:
- `usb_netif_aq.c` - Correct DHCP client implementation
- `usb_ncm_cb_aq.c` - Contains conflicting DHCP server code (should be removed)

### TinyUSB Integration
- Uses TinyUSB v0.18+ via esp_tinyusb managed component
- Callbacks: `tud_mount_cb()` starts DHCP client, `tud_umount_cb()` stops it
- Network data flows through `tud_network_recv_cb()` and transmit functions

### Event System
- Uses ESP-IDF event loop for network state changes
- `IP_EVENT_ETH_GOT_IP` signals successful DHCP lease acquisition
- Semaphore-based waiting for IP address assignment

## Testing and Verification

### Host Configuration (Raspberry Pi)
```bash
# Enable DHCP on USB interface using NetworkManager
sudo nmcli connection modify usb0 ipv4.method shared

# Or using dnsmasq for more control
sudo dnsmasq --interface=usb0 --dhcp-range=192.168.7.10,192.168.7.100,12h
```

### Expected Log Output
```
usb_netif_aq: USB mounted, starting DHCP client
usb_netif_aq: GOT_IP: 192.168.7.x
app_manager_aq: USB link ready, starting MQTT
```

### Troubleshooting
- Check `esp_netif_attach()` structure alignment - must start with `esp_netif_driver_base_t`
- Verify MAC address consistency between descriptor and network initialization
- Monitor DHCP traffic on host: `sudo tcpdump -i usb0 -n 'udp port 67 or 68'`

## Dependencies

### ESP-IDF Components
- `esp_netif` - Network interface abstraction
- `esp_event` - Event system
- `freertos` - RTOS kernel
- `esp_tinyusb` - TinyUSB integration

### Managed Components
- `espressif__esp_tinyusb` - TinyUSB driver
- `espressif__tinyusb` - Core TinyUSB library

### Key Headers
- `esp_netif.h`, `esp_netif_types.h` - Network interface APIs
- `tinyusb.h`, `tinyusb_net.h` - TinyUSB APIs
- `tusb.h` - Core TinyUSB definitions