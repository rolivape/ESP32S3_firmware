#include "usb_netif_aq.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

static const char *TAG = "usb_netif_aq";

/**
 * @brief Driver context structure
 */
typedef struct {
    esp_netif_driver_ifconfig_t ifconfig;
    esp_netif_t *netif;
} usb_netif_driver_t;

static usb_netif_driver_t s_driver;
static bool s_link_up = false;
static SemaphoreHandle_t s_got_ip_sem = NULL;
static esp_ip4_addr_t s_ip_addr;
static usb_netif_cfg_aq_t s_netif_cfg;
static uint8_t s_mac_addr[6];

/**
 * @brief TinyUSB network receive callback
 */
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    usb_netif_driver_t *driver = (usb_netif_driver_t *)ctx;
    if (driver && driver->netif) {
        return esp_netif_receive(driver->netif, buffer, len, NULL);
    }
    return ESP_FAIL;
}

/**
 * @brief Transmit function for esp_netif
 */
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len)
{
    if (tinyusb_net_send_sync(buffer, len, NULL, portMAX_DELAY) == ESP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

void tud_mount_cb(void)
{
    s_link_up = true;
    ESP_LOGI(TAG, "USB mounted, starting DHCP client");
    if (s_driver.netif) {
        esp_err_t ret = esp_netif_dhcpc_start(s_driver.netif);
        ESP_LOGI(TAG, "DHCP client start result: %s", esp_err_to_name(ret));
    }
}

void tud_umount_cb(void)
{
    s_link_up = false;
    ESP_LOGI(TAG, "USB unmounted, stopping DHCP client");
    if (s_driver.netif) {
        esp_err_t ret = esp_netif_dhcpc_stop(s_driver.netif);
        ESP_LOGI(TAG, "DHCP client stop result: %s", esp_err_to_name(ret));
    }
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (s_driver.netif != event->esp_netif) {
        return;
    }
    ESP_LOGI(TAG, "GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
    s_ip_addr = event->ip_info.ip;
    xSemaphoreGive(s_got_ip_sem);
}

esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_netif_cfg = *cfg;
    s_got_ip_sem = xSemaphoreCreateBinary();
    if (s_got_ip_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &on_got_ip, NULL);
}

esp_err_t usb_netif_start_aq(void)
{
    esp_read_mac(s_mac_addr, ESP_MAC_ETH);
    s_mac_addr[0] |= 0x02;
    s_mac_addr[0] &= ~0x01;

    const tinyusb_config_t tusb_cfg = { .external_phy = false };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *usb_netif = esp_netif_new(&cfg);

    s_driver.ifconfig.transmit = usb_netif_transmit;
    s_driver.ifconfig.driver_free_rx_buffer = NULL;
    s_driver.ifconfig.handle = &s_driver;
    s_driver.netif = usb_netif;

    void* glue = esp_netif_get_io_driver(usb_netif);
    ESP_ERROR_CHECK(esp_netif_attach(usb_netif, glue));
    
    tinyusb_net_config_t net_cfg = {
        .on_recv_callback = usb_recv_callback,
        .free_tx_buffer = NULL,
        .user_context = &s_driver,
    };
    memcpy(net_cfg.mac_addr, s_mac_addr, sizeof(s_mac_addr));
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    if (s_netif_cfg.hostname) {
        ESP_ERROR_CHECK(esp_netif_set_hostname(usb_netif, s_netif_cfg.hostname));
    }

    return ESP_OK;
}

esp_err_t usb_netif_stop_aq(void)
{
    if (s_driver.netif) {
        esp_netif_destroy(s_driver.netif);
        s_driver.netif = NULL;
    }
    tinyusb_driver_uninstall();
    return ESP_OK;
}

esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = s_driver.netif;
    return ESP_OK;
}

bool usb_netif_is_link_up_aq(void)
{
    return s_link_up;
}

esp_err_t usb_netif_wait_got_ip_aq(TickType_t timeout, esp_ip4_addr_t *out_ip)
{
    if (xSemaphoreTake(s_got_ip_sem, timeout) == pdTRUE) {
        if (out_ip) {
            *out_ip = s_ip_addr;
        }
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
