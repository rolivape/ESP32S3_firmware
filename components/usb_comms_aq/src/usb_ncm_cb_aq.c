#include "esp_log.h"
#include "tusb.h"
#include "class/cdc/cdc_device.h"

static const char *TAG = "usb_ncm_cb_aq";

/**
 * @brief Invoked when a control request is received for the CDC class.
 *
 * This function is a weak-symbol callback in TinyUSB. By implementing it here,
 * we can intercept and handle CDC-NCM specific control requests that the default
 * driver does not support. This is crucial to prevent asserts in the TinyUSB
d * core when the host sends requests like SET_ETHERNET_PACKET_FILTER.
 *
 * @param rhport The port number
 * @param itf The interface number
 * @param request The control request
 * @return true if the request was handled, false otherwise.
 */
bool tud_cdc_control_request_cb(uint8_t rhport, uint8_t itf, tusb_control_request_t const * request)
{
    ESP_LOGD(TAG, "tud_cdc_control_request_cb: itf=%u, bRequest=0x%x", itf, request->bRequest);

    // Check if it's a class-specific request for an interface
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE)
    {
        ESP_LOGW(TAG, "Unhandled CDC-NCM class-specific control request: bRequest=0x%x", request->bRequest);
        
        // To prevent the assert, we claim to have handled the request by sending a zero-length status packet.
        // The host will receive an empty response, but the connection will remain stable.
        tud_control_status(rhport, request);
        return true;
    }

    // If it's not a class-specific request we want to handle, let the default driver process it.
    return false;
}

/**
 * @brief Invoked when new data is received from the USB host.
 * 
 * This is a standard CDC callback. We provide a stub implementation here
 * as it's required by the CDC driver, but our NCM data is handled
 * via tud_network_recv_cb.
 * 
 * @param itf interface index
 */
void tud_cdc_rx_cb(uint8_t itf)
{
  // Not used for NCM
}

/**
 * @brief Invoked when line state DTR & RTS change
 * 
 * @param itf interface index
 * @param dtr device terminal ready
 * @param rts request to send
 */
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  // Not used for NCM
}
