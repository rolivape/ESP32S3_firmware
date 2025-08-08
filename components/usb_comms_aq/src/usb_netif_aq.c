#include "usb_netif_aq.h"
#include "usb_ncm_cb_aq.h"
#include "usb_descriptors_aq.h"
#include "tinyusb.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/pbuf.h"

// --- Constants and Globals ---
static const char *TAG = "USB_NETIF_AQ";
esp_netif_t *s_netif_aq = NULL;
uint8_t s_mac_address[6] __attribute__((used));

// --- Event Base Definition ---
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) __attribute__((used));
void netif_driver_free_rx_buffer(void *h, void* buffer) __attribute__((used));

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+

esp_err_t usb_netif_aq_start(void) {
    ESP_LOGI(TAG, "Starting USB NCM network interface...");

    // Generate the MAC address string needed for the descriptor
    fill_mac_ascii_from_chip();

    ESP_LOGI(TAG, "Initializing TinyUSB stack with custom descriptors");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &tusb_desc_device_aq,
        .string_descriptor = tusb_string_descriptors,
        .configuration_descriptor = tusb_desc_configuration_aq,
        .external_phy = false
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // --- Inicialización de esp_netif para USB NCM ---
    if (!s_netif_aq) {
        esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
        base_cfg.if_desc = "usb-ncm";
        base_cfg.route_prio = 30;
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        cfg.base = &base_cfg;
        cfg.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH;
        s_netif_aq = esp_netif_new(&cfg);
        if (!s_netif_aq) {
            ESP_LOGE(TAG, "Failed to create esp_netif for USB NCM");
            return ESP_FAIL;
        }
    }
    // Registro del driver personalizado con esp_netif para NCM
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = NULL,
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_cfg));

    // The TinyUSB task is started automatically by the driver

    ESP_LOGI(TAG, "USB NCM interface started successfully.");
    return ESP_OK;
}

//--------------------------------------------------------------------+
// esp_netif glue
//--------------------------------------------------------------------+

/**
 * @brief Transmit function for the esp_netif driver.
 *
 * This function is called by the TCP/IP stack when it wants to send a packet.
 * It passes the packet to the TinyUSB NCM driver.
 */
esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
    // Check if the network interface is ready to transmit
    if (!tud_network_can_xmit(len)) {
        return ESP_FAIL;
    }
    // TinyUSB's NCM driver makes a copy of the buffer, so we don't need to wait.
    tud_network_xmit(buffer, len);
    return ESP_OK;
}

/**
 * @brief Free RX buffer function for the esp_netif driver.
 *
 * This function is called by the TCP/IP stack to free a received buffer
 * after it has been processed.
 */
void netif_driver_free_rx_buffer(void *h, void* buffer) {
    // The buffer was allocated by TinyUSB, so we use its free function.
    tud_network_recv_renew();
}

//--------------------------------------------------------------------+
// TinyUSB NCM Callbacks
//--------------------------------------------------------------------+

/**
 * @brief Invoked when a network packet is received from the USB host.
 *
 * This function passes the received data to the esp_netif stack for processing.
 */
bool tud_network_recv_cb(const uint8_t* data, uint16_t len)
{
    if (!s_netif_aq) return false;       // aún sin inicializar
    esp_netif_receive(s_netif_aq, (void*)data, len, NULL);
    return true;                          // TinyUSB puede reutilizar el buffer
}

/**
 * @brief Invoked when a network packet has been transmitted.
 *
 * This is a callback from the TinyUSB stack to indicate that a packet
 * sent with tud_network_xmit() has been successfully transmitted.
 * We don't need to do anything here, but the function must exist.
 */
uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    (void)dst;
    (void)ref;
    (void)arg;
    return 0;
}

// ...existing code...

//--------------------------------------------------------------------+
// TinyUSB Device Callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB device mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB device unmounted");
}

bool tud_set_interface_cb(uint8_t itf, uint8_t alt)
{
    if (itf == 1 && alt == 1) {    // alt-setting 1 = link activo
        esp_event_post(USB_NET_EVENTS, USB_NET_UP, NULL, 0, 0);
    }
    return true;                             // siempre aceptar
}


/**
 * @brief Gets the handle for the USB network interface.
 */
esp_netif_t* usb_netif_get_handle(void) {
    return s_netif_aq;
}
