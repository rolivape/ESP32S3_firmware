#pragma once
#include <stddef.h>
#include <stdint.h>
#include "class/cdc/cdc_device.h"
#include "tusb.h"

// Device descriptor
extern const tusb_desc_device_t g_tusb_device_descriptor_aq;

// FS configuration descriptor (ECM/NCM según tu build)
extern const uint8_t g_tusb_fs_configuration_descriptor_aq[];

// String descriptors
extern const char * const g_tusb_string_descriptor_aq[];
extern const size_t g_tusb_string_descriptor_aq_count;

// Helper opcional para actualizar iMacAddress en runtime (si generas MAC al vuelo)
void usb_desc_set_mac_string(const char *mac12_hex);
