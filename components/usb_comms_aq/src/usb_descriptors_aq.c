#include "usb_descriptors_aq.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "USB_DESC_AQ";

//--------------------------------------------------------------------+
// String Descriptor Indexes
//--------------------------------------------------------------------+
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_MAC, // Required for NCM, iMACAddress
    STRID_NCM_INTERFACE,
};

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
const tusb_desc_device_t tusb_desc_device_aq = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0210, // USB 2.1 for NCM support
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A, // Espressif Inc.
    .idProduct          = 0x4002, // Custom Product ID for AquaController
    .bcdDevice          = 0x0100, // Device version
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1
};

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum {
    ITF_NUM_CDC_NCM_CTRL = 0,
    ITF_NUM_CDC_NCM_DATA,
    ITF_NUM_TOTAL
};

#define EPNUM_NET_NOTIF     0x81
#define EPNUM_NET_OUT       0x02
#define EPNUM_NET_IN        0x82

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

const uint8_t tusb_desc_configuration_aq[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC_NCM_CTRL, STRID_NCM_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NCM_MAX_SEGMENT_SIZE),
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// Static storage for the MAC address as an ASCII string (12 chars + null)
static char mac_str_ascii[13];

// Array of standard string descriptors
const char* tusb_string_descriptors[] = {
    [STRID_LANGID]        = (const char[]){0x09, 0x04},
    [STRID_MANUFACTURER]  = "AquaController",
    [STRID_PRODUCT]       = "NCM USB Device",
    [STRID_SERIAL]        = "1234567890AB",
    [STRID_MAC]           = mac_str_ascii,
    [STRID_NCM_INTERFACE] = "AquaController NCM",
};

/**
 * @brief Generates the MAC address string from the chip's efuse.
 *
 * Reads the base MAC, creates a locally-administered address, and formats
 * it as a 12-character hexadecimal ASCII string. This is a critical
 * step for the NCM interface to be recognized correctly by the host OS.
 */
void fill_mac_ascii_from_chip(void) {
    uint8_t base_mac[6];
    uint8_t local_mac[6];

    // Get base MAC address from EFUSE
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

    // Create a locally-administered MAC address
    // (set the second-least significant bit of the first octet)
    local_mac[0] = base_mac[0] | 0x02;
    local_mac[1] = base_mac[1];
    local_mac[2] = base_mac[2];
    local_mac[3] = base_mac[3];
    local_mac[4] = base_mac[4];
    // XORing the last byte provides some variation from the base MAC
    local_mac[5] = base_mac[5] ^ 0x55;

    // Format the MAC address into the ASCII string buffer
    snprintf(mac_str_ascii, sizeof(mac_str_ascii), "%02X%02X%02X%02X%02X%02X",
             local_mac[0], local_mac[1], local_mac[2], local_mac[3], local_mac[4], local_mac[5]);

    ESP_LOGI(TAG, "Generated MAC for USB Descriptor: %s", mac_str_ascii);
}
