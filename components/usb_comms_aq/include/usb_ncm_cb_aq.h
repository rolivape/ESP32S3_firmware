#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// This header is intentionally left blank.
// The functions in usb_ncm_cb_aq.c are weak-symbol callbacks that are
// automatically used by the TinyUSB stack when the object file is linked.
// Including this header into another source file is sufficient to ensure
// the linker includes the necessary symbols.

#ifdef __cplusplus
}
#endif
