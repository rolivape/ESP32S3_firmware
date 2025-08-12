// usb_descriptors_aq.c - VERSION CORREGIDA
#include <string.h>
#include "tusb.h"
#include "usb_descriptors_aq.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
const tusb_desc_device_t g_tusb_device_descriptor_aq = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC, // IAD
    .bDeviceSubClass    = 0x02,
    .bDeviceProtocol    = 0x01,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,   // Espressif
    .idProduct          = 0x4021,   
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

//--------------------------------------------------------------------+
// Configuration Descriptor (NCM)
//--------------------------------------------------------------------+
enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

const uint8_t g_tusb_fs_configuration_descriptor_aq[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // CDC-NCM: _itfnum, _desc_stridx, _mac_stridx, _ep_notif, _ep_notif_size, _epout, _epin, _epsize, _maxsegmentsize
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC, 0, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64, 1514),
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
static char s_mac_str[13] = "020000000000"; // Will be updated at runtime

// String descriptor array - CORREGIDO para esp_tinyusb
const char * const g_tusb_string_descriptor_aq[] = {
    (char[]){0x09, 0x04},        // 0: Language (0x0409 = English US)
    "AquaController",            // 1: Manufacturer
    "ESP32-S3 USB NCM",          // 2: Product  
    "AC-ESP32S3-001",           // 3: Serial Number
    s_mac_str,                   // 4: MAC Address (NCM uses this)
};

const size_t g_tusb_string_descriptor_aq_count = sizeof(g_tusb_string_descriptor_aq)/sizeof(g_tusb_string_descriptor_aq[0]);

void usb_desc_set_mac_string(const char *mac12_hex) {
    if (!mac12_hex) return;
    strncpy(s_mac_str, mac12_hex, sizeof(s_mac_str) - 1);
    s_mac_str[12] = '\0';
}

//--------------------------------------------------------------------+
// NOTA: Los callbacks tud_descriptor_*_cb() son manejados por esp_tinyusb
// No necesitamos definirlos aquí - esp_tinyusb los implementa usando 
// los descriptores que pasamos en tinyusb_config_t
//--------------------------------------------------------------------+