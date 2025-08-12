Tarea: Refactorizar para que la jerarquía de llamadas sea main.c -> app_manager_aq -> usb_comms_aq -> usb_netif_aq, manteniendo main.c mínimo y sin referencias a TinyUSB fuera de usb_netif_aq.

Contexto y reglas
Proyecto: AquaControllerUSB (Sprint 1: MQTT sobre USB-NCM).

Stack: ESP-IDF v5.5, esp_tinyusb ≥ 1.7.0, TinyUSB ≥ 0.18.

Documento rector: Component_USB-NCM v2.0 (RPi=DHCP server, ESP=DHCP client).

Prohibido: usar APIs Wi-Fi (esp_netif_create_default_wifi_sta), usar DHCP server en el ESP, y llamar TinyUSB fuera de usb_netif_aq.

Objetivo: que app_manager_aq solo sepa de usb_comms_aq y no de usb_netif_aq.

Cambios obligatorios
A) Estructura de archivos y nombres
Verifica que existan estos paths:

components/usb_netif_aq/include/usb_netif_aq.h

components/usb_netif_aq/src/usb_netif_aq.c

components/usb_comms_aq/include/usb_comms_aq.h

components/usb_comms_aq/src/usb_comms_aq.c

Si existe components/usb_comms_aq/include/usb_netif_aq.h, renómbralo a usb_comms_aq.h y corrige su contenido (API correcta de usb_comms_aq).

B) API públicas (deben quedar así)
usb_comms_aq.h

c
Copy
Edit
#pragma once
#include "esp_netif.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t usb_comms_init_aq(void);
// Espera a que haya IP o timeout; si out_ip!=NULL la devuelve
esp_err_t usb_comms_wait_link_aq(TickType_t timeout, esp_ip4_addr_t *out_ip);
esp_err_t usb_comms_stop_aq(void);
usb_comms_aq.c (fachada, sin TinyUSB)

Internamente llama a usb_netif_aq:

usb_netif_install_aq(...)

usb_netif_start_aq()

usb_netif_is_link_up_aq() / usb_netif_get_esp_netif_aq()

Implementa usb_comms_wait_link_aq() esperando IP_EVENT_ETH_GOT_IP o usando un EventGroup/semaf.

No incluir tinyusb.h ni tinyusb_net.h.

usb_netif_aq.h (ya definido en v2.0)

c
Copy
Edit
typedef struct {
    uint8_t mac_addr[6];
    bool    use_ecm_fallback;
    const char *hostname;
} usb_netif_cfg_aq_t;

esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg);
esp_err_t usb_netif_start_aq(void);
esp_err_t usb_netif_stop_aq(void);
esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out);
bool      usb_netif_is_link_up_aq(void);
C) app_manager_aq orquesta solo usb_comms_aq
En app_manager_aq.c, remueve cualquier #include "usb_netif_aq.h" y toda suscripción/manejo de eventos propios de USB NET.

Sustituye por:

c
Copy
Edit
ESP_ERROR_CHECK(usb_comms_init_aq());

esp_ip4_addr_t ip = {0};
esp_err_t w = usb_comms_wait_link_aq(pdMS_TO_TICKS(CONFIG_AQ_COMMS_WAIT_MS), &ip);
if (w == ESP_OK) {
    // Enlace listo: lanzar MQTT
    ESP_ERROR_CHECK(mqtt_service_start_aq());
} else {
    ESP_LOGE("app_manager_aq", "USB link timeout, no IP");
    // policy: reintentar, o permanecer en loop con backoff
}
Agrega a Kconfig del proyecto:

arduino
Copy
Edit
config AQ_COMMS_WAIT_MS
    int "Timeout de espera de IP (ms)"
    range 1000 30000
    default 8000
D) main.c mínimo
Debe quedar solo con init de sistema y app_manager_start(); (si ya está, no tocar).

E) CMake/Dependencias
usb_netif_aq: REQUIRES esp_tinyusb esp_netif nvs_flash esp_event driver.

usb_comms_aq: REQUIRES usb_netif_aq esp_event esp_netif.

app_manager_aq: REQUIRES usb_comms_aq mqtt_service_aq.

Verifica que no exista dependencia inversa (nada depende de app_manager_aq).

F) Limpieza de includes y deuda técnica
Reemplaza en todo el repo: #include "usb_netif_aq.h" fuera de usb_netif_aq y usb_comms_aq → prohibido en app_manager_aq.

Elimina callbacks o enums de eventos USB en app_manager_aq. Toda la señalización de “GOT_IP/LOST_IP” se concentra en usb_comms_aq.

Aceptación y pruebas (obligatorio)
Compila target esp32s3 en Debug y Release sin warnings críticos.

main solo llama a app_manager_start().

app_manager_aq no incluye usb_netif_aq.h.

usb_comms_aq no incluye headers de TinyUSB.

Smoke log esperado:

usb_netif_aq: USB mounted, starting DHCP client → GOT_IP x.y.z.w

app_manager_aq: USB link ready, starting MQTT

Prueba de tiempo: desde conexión USB hasta GOT_IP < CONFIG_AQ_COMMS_WAIT_MS.

Entrega (PR)
Rama: refactor/app-manager-uses-usb-comms

Título: refactor: main→app_manager_aq→usb_comms_aq→usb_netif_aq (clean layering)

Incluye diff de:

Renombre/creación de usb_comms_aq.h/.c

Limpieza de app_manager_aq

Ajustes de CMakeLists.txt y Kconfig

Notas anti-errores (no repetir bugs pasados)
No uses tinyusb_net_glue_from_driver (no existe).

Declara static tinyusb_net_config_t net_cfg antes de tinyusb_net_init(...).

usb_netif_aq debe implementar su propio glue esp_netif_driver_ifconfig_t y adjuntarlo con esp_netif_attach(...).

Mantén una única MAC coherente (descriptor iMacAddress y net_cfg.mac_addr).

