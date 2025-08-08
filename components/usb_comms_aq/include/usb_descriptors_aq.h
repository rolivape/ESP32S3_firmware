/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// Extern declarations for the USB descriptors defined in usb_descriptors_aq.c
extern const tusb_desc_device_t desc_device_aq;
extern const uint8_t desc_configuration_aq[];

/**
 * @brief Fills the ASCII MAC address string from the chip's efuse.
 *
 * This function reads the base MAC address from the ESP32's efuse,
 * creates a locally-administered MAC address, and formats it as a
 * null-terminated ASCII string for use in the USB descriptor.
 */
void fill_mac_ascii_from_chip(void);

/**
 * @brief TinyUSB callback invoked to get a string descriptor.
 *
 * This function is called by the TinyUSB stack when the host requests
 * a string descriptor. It handles returning the language ID, manufacturer,
* product, serial number, and the dynamically generated MAC address.
 *
 * @param index The index of the requested string descriptor.
 * @param langid The language ID.
 * @return A pointer to the UTF-16LE encoded string descriptor.
 */
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid);

#ifdef __cplusplus
}
#endif
