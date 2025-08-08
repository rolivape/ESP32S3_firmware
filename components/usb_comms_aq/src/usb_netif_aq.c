#include "usb_netif_aq.h"
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
static esp_netif_t *s_netif_aq = NULL;
static uint8_t s_mac_address[6];

// --- Event Base Definition ---
ESP_EVENT_DEFINE_BASE(USB_NET_EVENTS);

// --- Forward Declarations ---
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len);
static void netif_driver_free_rx_buffer(void *h, void* buffer);

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
static esp_err_t netif_driver_transmit_aq(void *h, void *buffer, size_t len) {
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
static void netif_driver_free_rx_buffer(void *h, void* buffer) {
    // The buffer was allocated by TinyUSB, so we use its free function.
    tud_network_recv_renew();
}

//--------------------------------------------------------------------+
// TinyUSB NCM Callbacks
//--------------------------------------------------------------------+

/**
 * @brief Invoked when the NCM network interface is initialized.
 *
 * This is the primary place where we set up the esp_netif instance,
 * as it's only called when the USB host has configured the device.
 * This function now configures and starts the DHCP server.
 */
void tud_network_init_cb(void) {
    ESP_LOGI(TAG, "NCM network interface initialized");

    // --- Set up the esp_netif stack for DHCP server ---
    const esp_netif_ip_info_t ip_info = {
        .ip = { .addr = ESP_IP4TOADDR( 192, 168, 7, 1) },
        .gw = { .addr = ESP_IP4TOADDR( 192, 168, 7, 1) },
        .netmask = { .addr = ESP_IP4TOADDR( 255, 255, 255, 0) },
    };

    const esp_netif_inherent_config_t inherent_cfg = {
        .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
        .ip_info = &ip_info,
        .if_key = "USB_NCM",
        .if_desc = "usb_ncm_netif",
        .route_prio = 30
    };

    esp_netif_config_t cfg = {
        .base = &inherent_cfg,
        .driver = NULL, // Driver is set later
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    s_netif_aq = esp_netif_new(&cfg);
    if (!s_netif_aq) {
        ESP_LOGE(TAG, "Failed to create esp_netif instance");
        return;
    }

    // --- Set up the driver IO functions ---
    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle = (void *)1, // Non-NULL handle
        .transmit = netif_driver_transmit_aq,
        .driver_free_rx_buffer = netif_driver_free_rx_buffer
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(s_netif_aq, &driver_ifconfig));

    // --- Set MAC address ---
    ESP_ERROR_CHECK(esp_read_mac(s_mac_address, ESP_MAC_WIFI_STA));
    s_mac_address[0] |= 0x02; // Set as locally administered address
    s_mac_address[5] ^= 0x55;
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif_aq, s_mac_address));
}

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
 * @brief Invoked when the network link state changes (e.g., cable connected/disconnected).
 *
 * This function starts or stops the esp_netif actions and the DHCP server
 * based on the link status.
 */
void tud_network_link_state_cb(bool itf_up) {
    if (itf_up) {
        ESP_LOGI(TAG, "NCM network link is UP");
    } else {
        ESP_LOGI(TAG, "NCM network link is DOWN");
        esp_event_post(USB_NET_EVENTS, USB_NET_DOWN, NULL, 0, portMAX_DELAY);
    }
}

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
