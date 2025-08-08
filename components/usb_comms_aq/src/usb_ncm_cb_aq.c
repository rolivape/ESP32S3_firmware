#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "usb_netif_aq.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "tusb.h"
#include "class/cdc/cdc_device.h"
#include "usb_descriptors_internal.h" // For ITF_NUM_CDC_NCM_CTRL
#ifdef __cplusplus
}
#endif

extern esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len);
extern void netif_driver_free_rx_buffer(void *h, void* buffer);
extern esp_netif_t *s_netif_aq;
extern uint8_t s_mac_address[6];
extern const char *TAG;

void tud_network_init_cb(void) {
    ESP_LOGI(TAG, "Entrando a tud_network_init_cb");
    ESP_LOGI(TAG, "NCM network interface initialized");

    // --- Create esp_netif instance ---
    ESP_LOGI(TAG, "Creando instancia esp_netif...");
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_aq = esp_netif_new(&cfg);
    ESP_LOGI(TAG, "esp_netif_new OK: %p", s_netif_aq);

    // --- Set up driver IO functions ---
    ESP_LOGI(TAG, "Configurando driver IO functions...");
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = (void *)1,
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_ifconfig));
    ESP_LOGI(TAG, "esp_netif_set_driver_config OK");

    // --- Attach the driver to the netif ---
    ESP_LOGI(TAG, "Adjuntando driver a netif...");
    void* driver_handle = (void*)&driver_ifconfig;
    ESP_ERROR_CHECK(esp_netif_attach(s_netif_aq, driver_handle));
    ESP_LOGI(TAG, "esp_netif_attach OK");

    // --- Stop DHCP client and set static IP for the device ---
    ESP_LOGI(TAG, "Parando DHCP client y seteando IP estatica...");
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_netif_aq));
    esp_netif_ip_info_t ip_info = {
        .ip = { .addr = ESP_IP4TOADDR(192, 168, 7, 1) },
        .gw = { .addr = ESP_IP4TOADDR(192, 168, 7, 1) },
        .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) },
    };
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif_aq, &ip_info));
    ESP_LOGI(TAG, "Set static IP for device: 192.168.7.1");

    // --- Start DHCP server for the host using public esp_netif API ---
    ESP_LOGI(TAG, "Iniciando DHCP server...");
    esp_netif_dhcp_option_id_t opt_op = ESP_NETIF_OP_SET;
    uint8_t offer_dns = 1;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(s_netif_aq, opt_op, ESP_NETIF_DOMAIN_NAME_SERVER, &offer_dns, sizeof(offer_dns)));
    ESP_LOGI(TAG, "Opcion DNS para DHCP OK");

    uint32_t lease_time_mins = 120;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(s_netif_aq, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease_time_mins, sizeof(lease_time_mins)));
    ESP_LOGI(TAG, "Lease time para DHCP OK");

    esp_err_t start_err = esp_netif_dhcps_start(s_netif_aq);
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DHCP server: %s", esp_err_to_name(start_err));
    } else {
        ESP_LOGI(TAG, "DHCP server started on %s", esp_netif_get_ifkey(s_netif_aq));
    }

    // --- Set MAC address ---
    ESP_LOGI(TAG, "Seteando MAC address...");
    ESP_ERROR_CHECK(esp_read_mac(s_mac_address, ESP_MAC_WIFI_STA));
    s_mac_address[0] |= 0x02; // Set as locally administered address
    s_mac_address[5] ^= 0x55;
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, s_mac_address));
    ESP_LOGI(TAG, "MAC address seteada");
}


void tud_network_link_state_cb(bool itf_up) {
    if (itf_up) {
        ESP_LOGI(TAG, "NCM network link is UP");
    } else {
        ESP_LOGI(TAG, "NCM network link is DOWN");
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, portMAX_DELAY);
    }
}

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

    // Check if it's a class-specific request for the NCM control interface
    // and if it's a request we specifically want to handle to prevent asserts.
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE)
    {
        ESP_LOGW(TAG, "Unhandled CDC-NCM class-specific control request: bRequest=0x%x", request->bRequest);
        
        // To prevent the assert, we claim to have handled the request by sending a zero-length status packet.
        // The host will receive an empty response, but the connection will remain stable.
        // Specifically handle requests for the NCM control interface.
        if (itf == ITF_NUM_CDC_NCM_CTRL) {
            tud_control_status(rhport, request);
            return true;
        } else {
            // If it's a class-specific request for another CDC interface,
            // let the default handler process it (or return false if no default handler).
            return false;
        }
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

// Force linker to include tud_network_recv_cb implementation
extern bool tud_network_recv_cb(const uint8_t*, uint16_t);
static bool (*force_link_tud_network_recv_cb_ptr)(const uint8_t*, uint16_t) = tud_network_recv_cb;
