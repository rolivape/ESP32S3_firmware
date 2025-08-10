#include "usb_descriptors_aq.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "USB_DESC_AQ";

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
const tusb_desc_device_t tusb_desc_device_aq = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A, // Espressif Inc.
    .idProduct          = 0x4002, // Custom Product ID for AquaController NCM
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1
};

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_NCM_DESC_LEN)

const uint8_t tusb_desc_configuration_aq[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_CDC_NCM_CTRL, STRID_NCM_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NCM_MAX_SEGMENT_SIZE),
};

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
static char mac_str_ascii[13];

const char* tusb_string_descriptors[] = {
    [STRID_LANGID]        = (const char[]){0x09, 0x04},
    [STRID_MANUFACTURER]  = "AquaController",
    [STRID_PRODUCT]       = "NCM USB Device",
    [STRID_SERIAL]        = "1234567890AB",
    [STRID_MAC]           = mac_str_ascii,
    [STRID_NCM_INTERFACE] = "AquaController NCM",
};

//--------------------------------------------------------------------+
// MAC Address Logic
//--------------------------------------------------------------------+
extern uint8_t tud_network_mac_address[6];

void fill_mac_ascii_from_chip(void) {
    uint8_t base_mac[6];
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);

    tud_network_mac_address[0] = base_mac[0] | 0x02;
    tud_network_mac_address[1] = base_mac[1];
    tud_network_mac_address[2] = base_mac[2];
    tud_network_mac_address[3] = base_mac[3];
    tud_network_mac_address[4] = base_mac[4];
    tud_network_mac_address[5] = base_mac[5] ^ 0x55;

    snprintf(mac_str_ascii, sizeof(mac_str_ascii), "%02X%02X%02X%02X%02X%02X",
             tud_network_mac_address[0], tud_network_mac_address[1], tud_network_mac_address[2],
             tud_network_mac_address[3], tud_network_mac_address[4], tud_network_mac_address[5]);

    ESP_LOGI(TAG, "Generated MAC for USB Descriptor: %s", mac_str_ascii);
}
