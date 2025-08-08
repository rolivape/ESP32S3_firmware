#ifndef USB_DESCRIPTORS_AQ_H
#define USB_DESCRIPTORS_AQ_H

#include "tusb.h"

// Extern declarations for the descriptors defined in usb_descriptors_aq.c
extern const tusb_desc_device_t tusb_desc_device_aq;
extern const uint8_t tusb_desc_configuration_aq[];
extern const char* tusb_string_descriptors[];

// Function to initialize MAC address string
void fill_mac_ascii_from_chip(void);

#endif // USB_DESCRIPTORS_AQ_H
