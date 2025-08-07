/*
 * AquaController USB Descriptors
 *
 * This file contains the custom USB descriptors for the AquaController device,
 * ensuring correct enumeration as a CDC-NCM device.
 */

#include "tusb.h"
#include "sdkconfig.h"
#include "usb_comms_aq.h" // For descriptor prototypes
#include "tinyusb.h"

enum {
    ITF_NUM_CDC_NCM = 0,
    ITF_NUM_TOTAL
};

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device_aq = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200, // USB 2.0
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A, // Espressif Inc.
    .idProduct          = 0x4002, // Unique product ID for AquaController NCM
    .bcdDevice          = 0x0100, // Version 1.0
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)
#define EPNUM_NET_NOTIF     0x81
#define EPNUM_NET_OUT       0x02
#define EPNUM_NET_IN        0x82

uint8_t const desc_configuration_aq[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    // Interface number, string index, MAC address string index, EP notification address and size, EP data address (out, in) and size, max segment size.
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC_NCM, 4, 5, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE, 1514),
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
// Array of pointer to string descriptors
char const* string_desc_arr_aq[] = {
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "AquaController",    // 1: Manufacturer
    "NCM USB Device",    // 2: Product
    "123456",            // 3: Serials, should be unique
    "02:00:00:00:00:01", // 4: CDC-NCM MAC String
};

static uint16_t _desc_str[32];

// Callback invoked when received GET STRING DESCRIPTOR request
// See: https://github.com/hathach/tinyusb/blob/master/examples/device/cdc_msc/src/usb_descriptors.c
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[0], string_desc_arr_aq[0], 2);
        chr_count = 1;
    } else {
        // Convert ASCII string into UTF-16
        if (!(index < sizeof(string_desc_arr_aq) / sizeof(string_desc_arr_aq[0]))) {
            return NULL;
        }

        const char* str = string_desc_arr_aq[index];
        chr_count = strlen(str);
        if (chr_count > 31) {
            chr_count = 31;
        }

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (in bytes), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device_aq;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations
  return desc_configuration_aq;
}

void tinyusb_set_descriptors(const tinyusb_config_t *config)
{
    // This is a dummy implementation to satisfy the linker.
    // The actual implementation is in the esp_tinyusb component, but it's not being included.
    // The descriptors are already set in the tud_descriptor_*_cb functions.
}
