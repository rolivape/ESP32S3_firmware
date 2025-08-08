#pragma once

// Interface number enums
enum {
    ITF_NUM_CDC_NCM_CTRL = 0,
    ITF_NUM_CDC_NCM_DATA,
    ITF_NUM_TOTAL
};

// Endpoint numbers
#define EPNUM_NET_NOTIF     0x81
#define EPNUM_NET_OUT       0x02
#define EPNUM_NET_IN        0x82
