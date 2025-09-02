#include "usb_netif_aq.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "tinyusb.h"
#include "tinyusb_net.h"
#include "tusb.h"
#include "usb_descriptors_aq.h"

static const char *TAG = "usb_netif_aq";

#define RX_QUEUE_SIZE 10
#define RX_TASK_STACK_SIZE 8192
#define USB_CONNECTED_BIT (1 << 0)

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
static TaskHandle_t s_usb_device_task_handle = NULL;  // CRITICAL: USB device task handle
static EventGroupHandle_t s_usb_event_group = NULL;

// Forward declarations
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len);
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx);
static void usb_netif_free_rx(void *h, void *buffer);
static esp_err_t usb_post_attach(esp_netif_t *esp_netif, void *args);
static void usb_rx_task(void *arg);
static void usb_device_task(void *param);  // CRITICAL: USB device task

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
    ESP_LOGI(TAG, "USB RX callback: %d bytes", len);
    if (s_rx_queue) {
        rx_packet_t pkt = { .buffer = malloc(len), .len = len };
        if (pkt.buffer) {
            memcpy(pkt.buffer, buffer, len);
            if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
                ESP_LOGE(TAG, "RX Queue full, packet dropped");
                free(pkt.buffer);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "Packet queued for netif processing");
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to allocate buffer for RX packet");
            return ESP_FAIL;
        }
    }
    ESP_LOGW(TAG, "RX queue not available, dropping packet");
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
            ESP_LOGI(TAG, "RX task processing %d bytes", pkt.len);
            if (s_driver_context.netif) {
                esp_err_t ret = esp_netif_receive(s_driver_context.netif, pkt.buffer, pkt.len, NULL);
                ESP_LOGI(TAG, "esp_netif_receive result: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGW(TAG, "Netif not available, dropping packet");
                free(pkt.buffer);
            }
        }
    }
}

// ========== CRITICAL: USB DEVICE TASK ==========
// Without this task, TinyUSB cannot process USB events and USB-NCM will not work
static void usb_device_task(void *param) {
    ESP_LOGI(TAG, "USB device task started - CRITICAL for USB-NCM functionality");
    
    // Wait for USB mount with timeout
    int mount_timeout = 50; // 5 seconds
    while (!tud_mounted() && mount_timeout > 0) {
        ESP_LOGD(TAG, "Waiting for USB mount... %d", mount_timeout);
        vTaskDelay(pdMS_TO_TICKS(100));
        mount_timeout--;
    }
    
    if (tud_mounted()) {
        ESP_LOGI(TAG, "USB mounted successfully!");
    } else {
        ESP_LOGW(TAG, "USB mount timeout - will continue processing");
    }
    
    // MAIN LOOP - ABSOLUTELY CRITICAL FOR USB FUNCTIONALITY
    while (1) {
        tud_task(); // <-- WITHOUT THIS LINE, USB-NCM WILL NOT WORK
        vTaskDelay(pdMS_TO_TICKS(1)); // Must be 1ms or less for proper USB timing
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

// ========== TINYUSB CALLBACKS ==========
// Note: The USB device task with tud_task() is handled by esp_tinyusb managed component

// Callback when USB mounts
void tud_mount_cb(void) {
    ESP_LOGI(TAG, "=== USB MOUNTED EVENT ===");
    s_link_up = true;
    if (s_usb_event_group) {
        xEventGroupSetBits(s_usb_event_group, USB_CONNECTED_BIT);
    }
    if (s_driver_context.netif) {
        ESP_LOGI(TAG, "USB mounted, waiting 1s then starting DHCP client");
        
        // Give time for network stack to settle
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Check current DHCP status
        esp_netif_dhcp_status_t dhcp_status;
        esp_netif_dhcpc_get_status(s_driver_context.netif, &dhcp_status);
        ESP_LOGI(TAG, "DHCP status before restart: %d", dhcp_status);
        
        // Ensure interface is up before starting DHCP
        esp_netif_action_start(s_driver_context.netif, NULL, 0, NULL);
        
        // Force stop and restart DHCP client with more aggressive settings
        esp_netif_dhcpc_stop(s_driver_context.netif);
        vTaskDelay(pdMS_TO_TICKS(500));  // Longer delay
        
        // Configure DHCP options for faster discovery
        uint32_t dns = esp_netif_htonl(0x08080808); // Google DNS as fallback
        esp_netif_dhcpc_option(s_driver_context.netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dns, sizeof(dns));
        
        // Start DHCP client with explicit configuration
        esp_err_t ret = esp_netif_dhcpc_start(s_driver_context.netif);
        ESP_LOGI(TAG, "DHCP client start result: %s", esp_err_to_name(ret));
        
        // Check DHCP status after start
        esp_netif_dhcpc_get_status(s_driver_context.netif, &dhcp_status);
        ESP_LOGI(TAG, "DHCP status after start: %d", dhcp_status);
        
        // Longer delay for DHCP discovery to begin
        vTaskDelay(pdMS_TO_TICKS(2000));
        ESP_LOGI(TAG, "DHCP discovery should be active now - forcing refresh");
        
        // Force trigger DHCP by restarting after delay
        ESP_LOGI(TAG, "Triggering additional DHCP restart to force packet transmission");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_netif_dhcpc_stop(s_driver_context.netif);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_netif_dhcpc_start(s_driver_context.netif);
    } else {
        ESP_LOGE(TAG, "CRITICAL: s_driver_context.netif is NULL - DHCP cannot start!");
    }
}

// Callback when USB unmounts
void tud_umount_cb(void) {
    ESP_LOGW(TAG, "=== USB UNMOUNTED EVENT ===");
    s_link_up = false;
    if (s_usb_event_group) {
        xEventGroupClearBits(s_usb_event_group, USB_CONNECTED_BIT);
    }
    if (s_driver_context.netif) {
        ESP_LOGI(TAG, "USB unmounted, stopping DHCP client");
        esp_netif_dhcpc_stop(s_driver_context.netif);
    }
}

// Network init is handled by esp_tinyusb managed component

// Callback for link state changes (may not be called in esp_tinyusb managed component)
void tud_network_link_state_cb(bool state) {
    ESP_LOGI(TAG, "Network link: %s", state ? "UP" : "DOWN");
    if (state && s_driver_context.netif) {
        // When link is UP, ensure DHCP client is started
        esp_netif_action_start(s_driver_context.netif, 0, 0, NULL);
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
    
    // Create synchronization primitives
    s_got_ip_sem = xSemaphoreCreateBinary();
    if (s_got_ip_sem == NULL) return ESP_ERR_NO_MEM;
    
    s_rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(rx_packet_t));
    if (s_rx_queue == NULL) {
        vSemaphoreDelete(s_got_ip_sem);
        return ESP_ERR_NO_MEM;
    }
    
    s_usb_event_group = xEventGroupCreate();
    if (s_usb_event_group == NULL) {
        vSemaphoreDelete(s_got_ip_sem);
        vQueueDelete(s_rx_queue);
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

    // Configure esp_netif for USB-NCM with explicit DHCP client configuration
    esp_netif_inherent_config_t usb_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    usb_netif_config.if_key = "USB_NCM";
    usb_netif_config.if_desc = "usb_ncm";
    usb_netif_config.route_prio = 50;
    
    esp_netif_config_t cfg = {
        .base = &usb_netif_config,
        .driver = NULL,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    
    esp_netif_t *usb_netif = esp_netif_new(&cfg);
    if (!usb_netif) {
        ESP_LOGE(TAG, "esp_netif_new failed");
        return ESP_FAIL;
    }
    
    // Force DHCP client mode and IPv4 configuration
    ESP_LOGI(TAG, "Configuring USB-NCM netif for DHCP client mode");
    esp_netif_dhcp_status_t dhcp_status;
    esp_netif_dhcpc_get_status(usb_netif, &dhcp_status);
    ESP_LOGI(TAG, "Initial DHCP status: %d", dhcp_status);
    
    // Ensure DHCP client is properly configured
    esp_err_t ret = esp_netif_dhcpc_stop(usb_netif);
    ESP_LOGI(TAG, "DHCP client stop result: %s", esp_err_to_name(ret));

    // Store netif reference BEFORE attaching driver
    s_driver_context.netif = usb_netif;
    
    // Configurar el driver para esp_netif
    static usb_netif_driver_t s_usb_drv;
    s_usb_drv.base.post_attach = usb_post_attach;
    s_usb_drv.impl = &s_driver_context;

    ESP_ERROR_CHECK(esp_netif_attach(usb_netif, &s_usb_drv));
    ESP_LOGI(TAG, "esp_netif attached, netif stored: %p", (void*)usb_netif);

    if (s_netif_cfg.hostname) {
        esp_netif_set_hostname(usb_netif, s_netif_cfg.hostname);
    }

    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
        .device_descriptor = &g_tusb_device_descriptor_aq,
        .string_descriptor = (const char**)g_tusb_string_descriptor_aq,
        .string_descriptor_count = g_tusb_string_descriptor_aq_count,
    };
    ESP_LOGI(TAG, "Initializing TinyUSB driver...");
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    
    // Note: tud_init() is handled internally by esp_tinyusb managed component
    // No need to call tud_init() manually when using tinyusb_driver_install()
    ESP_LOGI(TAG, "TinyUSB driver initialized via esp_tinyusb");

    tinyusb_net_config_t net_cfg = {
        .on_recv_callback = usb_recv_callback,
    };
    memcpy(net_cfg.mac_addr, s_mac_addr, 6);
    ESP_LOGI(TAG, "Initializing TinyUSB network with MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             s_mac_addr[0], s_mac_addr[1], s_mac_addr[2], s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]);
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    // Create RX task
    xTaskCreate(usb_rx_task, "usb_rx", RX_TASK_STACK_SIZE, NULL, 5, &s_rx_task_handle);
    
    // CRITICAL: Create USB device task - This is essential for USB-NCM functionality
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        usb_device_task,           // Task function
        "usb_device",              // Task name
        4096,                      // Stack size
        NULL,                      // Parameters
        configMAX_PRIORITIES - 2, // High priority
        &s_usb_device_task_handle, // Task handle
        0                          // Core 0 (can try Core 1 if issues)
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "CRITICAL: Failed to create USB device task!");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "CRITICAL: USB device task created - USB-NCM should now work");
    ESP_LOGI(TAG, "USB-NCM initialization complete");

    return ESP_OK;
}

esp_err_t usb_netif_stop_aq(void) {
    // Stop USB device task first
    if (s_usb_device_task_handle) {
        vTaskDelete(s_usb_device_task_handle);
        s_usb_device_task_handle = NULL;
        ESP_LOGI(TAG, "USB device task stopped");
    }
    
    if (s_rx_task_handle) {
        vTaskDelete(s_rx_task_handle);
        s_rx_task_handle = NULL;
    }
    
    if (s_rx_queue) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }
    
    if (s_usb_event_group) {
        vEventGroupDelete(s_usb_event_group);
        s_usb_event_group = NULL;
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

// ========== NETWORK CALLBACKS ==========
// The tinyusb_net callbacks are handled by the esp_tinyusb managed component
// We use the tinyusb_net layer which provides on_recv_callback mechanism
// This approach works with the existing esp_tinyusb architecture
