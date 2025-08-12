// components/usb_netif_aq/src/usb_descriptors_aq.c
#include <string.h>
#include "tusb.h"
#include "usb_descriptors_aq.h"

// --- Device Descriptor (VID/PID tomados de logs actuales) ---
const tusb_desc_device_t g_tusb_device_descriptor_aq = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC, // IAD (EFh)
    .bDeviceSubClass    = 0x02,
    .bDeviceProtocol    = 0x01,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,   // Espressif
    .idProduct          = 0x4021,   // (coincidir con tu PID real)
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// --- Strings: índice 1..N (esp_tinyusb maneja el idioma 0x0409) ---
static char s_mac_str[13] = "020000000000"; // iMacAddress (12 HEX, sin ':')
static const char *s_string_desc[] = {
    "AquaController",       // 1: Manufacturer
    "ESP32-S3 USB NET",     // 2: Product
    "AC-0001",              // 3: Serial (puedes generar uno)
    s_mac_str,               // 4: iMacAddress (usado por ECM/NCM)
};

const char * const g_tusb_string_descriptor_aq[] = {
    [0] = "AquaController",       // 1: Manufacturer
    [1] = "ESP32-S3 USB NET",     // 2: Product
    [2] = "AC-0001",              // 3: Serial (puedes generar uno)
    [3] = s_mac_str,               // 4: iMacAddress (usado por ECM/NCM)
};
const size_t g_tusb_string_descriptor_aq_count = sizeof(s_string_desc)/sizeof(s_string_desc[0]);

void usb_desc_set_mac_string(const char *mac12_hex)
{
    if (!mac12_hex) return;
    // Copiamos exactamente 12 chars (sin ':'). Asegura mayúsculas previamente si quieres.
    strncpy(s_mac_str, mac12_hex, sizeof(s_mac_str) - 1);
    s_mac_str[12] = '\0';
}
