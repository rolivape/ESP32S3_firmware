#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUSB_MCU                OPT_MCU_ESP32S3
#define CFG_TUSB_OS                 OPT_OS_FREERTOS
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#define CFG_TUSB_DEBUG              0

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUD_ENABLED             1
#define CFG_TUD_MAX_SPEED           TUSB_SPEED_HIGH
#define CFG_TUD_ENDPOINT0_SIZE      64

//--------------------------------------------------------------------
// USB-CDC CLASS CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUD_CDC                 1
#define CFG_TUD_ECM_RNDIS           1

#endif // _TUSB_CONFIG_H_