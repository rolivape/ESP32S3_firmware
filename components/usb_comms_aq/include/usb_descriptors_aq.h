#ifndef USB_DESCRIPTORS_AQ_H
#define USB_DESCRIPTORS_AQ_H

#include "tusb.h"

// Enum for String Descriptor Indexes
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_MAC,
    STRID_CDC_INTERFACE,
};

// Enum for Interface Numbers
enum {
    ITF_NUM_CDC_CTRL = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

// Enum for Endpoint Numbers
enum {
    EPNUM_NET_NOTIF = 0x81,
    EPNUM_NET_OUT   = 0x02,
    EPNUM_NET_IN    = 0x82,
};

// Extern declarations for the descriptors defined in usb_descriptors_aq.c
extern const tusb_desc_device_t tusb_desc_device_aq;
extern const uint8_t tusb_desc_configuration_aq[];
extern const char* tusb_string_descriptors[];

// Function to initialize MAC address string
void fill_mac_ascii_from_chip(void);

#endif // USB_DESCRIPTORS_AQ_H