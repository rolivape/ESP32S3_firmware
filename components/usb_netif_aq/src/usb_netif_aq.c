#include "usb_netif_aq.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "tinyusb_net.h"
#include "usb_descriptors_aq.h"

static const char *TAG = "usb_netif_aq";

#define RX_QUEUE_SIZE 10
#define RX_TASK_STACK_SIZE 8192

// Contexto del driver de bajo nivel (tinyusb)
typedef struct {
    esp_netif_t *netif;
} usb_driver_context_t;

// Handle del driver para esp_netif, debe tener base como primer miembro
typedef struct {
    esp_netif_driver_base_t base;
    usb_driver_context_t *impl;
} usb_netif_driver_t;

typedef struct {
    uint8_t *buffer;
    uint16_t len;
} rx_packet_t;

static usb_driver_context_t s_driver_context = {0};
static bool s_link_up = false;
static SemaphoreHandle_t s_got_ip_sem = NULL;
static esp_ip4_addr_t s_ip_addr;
static usb_netif_cfg_aq_t s_netif_cfg;
static uint8_t s_mac_addr[6];
static QueueHandle_t s_rx_queue = NULL;
static TaskHandle_t s_rx_task_handle = NULL;

// Forward declarations
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len);
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx);
static void usb_netif_free_rx(void *h, void *buffer);
static esp_err_t usb_post_attach(esp_netif_t *esp_netif, void *args);
static void usb_rx_task(void *arg);

// TX: from esp_netif -> USB
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len) {
    (void)h; // h es el handle de nuestro impl, s_driver_context
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(200)) == ESP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

// RX: from USB -> Queue
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx) {
    if (s_rx_queue) {
        rx_packet_t pkt = { .buffer = malloc(len), .len = len };
        if (pkt.buffer) {
            memcpy(pkt.buffer, buffer, len);
            if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
                ESP_LOGE(TAG, "RX Queue full, packet dropped");
                free(pkt.buffer);
                return ESP_FAIL;
            }
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to allocate buffer for RX packet");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static void usb_netif_free_rx(void *h, void *buffer) {
    (void)h;
    free(buffer);
}

static void usb_rx_task(void *arg) {
    rx_packet_t pkt;
    while (1) {
        if (xQueueReceive(s_rx_queue, &pkt, portMAX_DELAY) == pdTRUE) {
            if (s_driver_context.netif) {
                esp_netif_receive(s_driver_context.netif, pkt.buffer, pkt.len, NULL);
            } else {
                free(pkt.buffer);
            }
        }
    }
}

static esp_err_t usb_post_attach(esp_netif_t *esp_netif, void *args) {
    usb_netif_driver_t *drv = (usb_netif_driver_t *)args;
    drv->base.netif = esp_netif;
    drv->impl->netif = esp_netif; // Asignamos el netif a nuestro contexto

    const esp_netif_driver_ifconfig_t ifcfg = {
        .handle = drv->impl,
        .transmit = usb_netif_transmit,
        .driver_free_rx_buffer = usb_netif_free_rx,
    };

    return esp_netif_set_driver_config(esp_netif, &ifcfg);
}

void tud_mount_cb(void) {
    s_link_up = true;
    if (s_driver_context.netif) {
        ESP_LOGI(TAG, "USB mounted, starting DHCP client");
        esp_netif_dhcpc_start(s_driver_context.netif);
    }
}

void tud_umount_cb(void) {
    s_link_up = false;
    if (s_driver_context.netif) {
        ESP_LOGI(TAG, "USB unmounted, stopping DHCP client");
        esp_netif_dhcpc_stop(s_driver_context.netif);
    }
}

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (s_driver_context.netif != event->esp_netif) {
        return;
    }
    ESP_LOGI(TAG, "GOT_IP: " IPSTR, IP2STR(&event->ip_info.ip));
    s_ip_addr = event->ip_info.ip;
    xSemaphoreGive(s_got_ip_sem);
}

esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg) {
    if (cfg == NULL) return ESP_ERR_INVALID_ARG;
    s_netif_cfg = *cfg;
    s_got_ip_sem = xSemaphoreCreateBinary();
    if (s_got_ip_sem == NULL) return ESP_ERR_NO_MEM;
    s_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(rx_packet_t));
    if (s_rx_queue == NULL) {
        vSemaphoreDelete(s_got_ip_sem);
        return ESP_ERR_NO_MEM;
    }
    return esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &on_got_ip, NULL);
}

esp_err_t usb_netif_start_aq(void) {
    esp_read_mac(s_mac_addr, ESP_MAC_ETH);
    s_mac_addr[0] |= 0x02;
    s_mac_addr[0] &= ~0x01;

    char mac_str[13];
    snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
             s_mac_addr[0], s_mac_addr[1], s_mac_addr[2], s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]);
    usb_desc_set_mac_string(mac_str);

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *usb_netif = esp_netif_new(&cfg);
    if (!usb_netif) {
        ESP_LOGE(TAG, "esp_netif_new failed");
        return ESP_FAIL;
    }

    // Configurar el driver para esp_netif
    static usb_netif_driver_t s_usb_drv;
    s_usb_drv.base.post_attach = usb_post_attach;
    s_usb_drv.impl = &s_driver_context;

    ESP_ERROR_CHECK(esp_netif_attach(usb_netif, &s_usb_drv));

    if (s_netif_cfg.hostname) {
        esp_netif_set_hostname(usb_netif, s_netif_cfg.hostname);
    }

    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
        .device_descriptor = &g_tusb_device_descriptor_aq,
        .string_descriptor = (const char**)g_tusb_string_descriptor_aq,
        .string_descriptor_count = g_tusb_string_descriptor_aq_count,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_net_config_t net_cfg = {
        .on_recv_callback = usb_recv_callback,
    };
    memcpy(net_cfg.mac_addr, s_mac_addr, 6);
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    xTaskCreate(usb_rx_task, "usb_rx", RX_TASK_STACK_SIZE, NULL, 5, &s_rx_task_handle);

    return ESP_OK;
}

esp_err_t usb_netif_stop_aq(void) {
    if (s_rx_task_handle) {
        vTaskDelete(s_rx_task_handle);
        s_rx_task_handle = NULL;
    }
    if (s_rx_queue) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
    if (s_driver_context.netif) {
        esp_netif_destroy(s_driver_context.netif);
        s_driver_context.netif = NULL;
    }
    tinyusb_driver_uninstall();
    return ESP_OK;
}

esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out) {
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    *out = s_driver_context.netif;
    return ESP_OK;
}

bool usb_netif_is_link_up_aq(void) {
    return s_link_up;
}

esp_err_t usb_netif_wait_got_ip_aq(TickType_t timeout, esp_ip4_addr_t *out_ip) {
    if (xSemaphoreTake(s_got_ip_sem, timeout) == pdTRUE) {
        if (out_ip) *out_ip = s_ip_addr;
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
