# Prompt para Gemini CLI — `usb_netif_aq` (v2)

> **Plantilla\_Prompt.md aplicada**. Componente autocontenido, enfoque RPi=DHCP Server / ESP32-S3=DHCP Client. TinyUSB ≥ 0.18.

---

## 1) Tarea

Implementar el **ESQUELETO** del componente `usb_netif_aq` (wrapper **TinyUSB device** + clase **tinyusb\_net** NCM/ECM) para **ESP32-S3** (ESP‑IDF v5.5), exponiendo una interfaz ``** tipo Ethernet** con **cliente DHCP**.

> **No** integrar otros módulos (sin `app_manager_aq`, sin `mqtt_service_aq`).

## 2) Contexto y Jerarquía

- Proyecto: **AquaControllerUSB** (Sprint 1: **MQTT sobre USB‑NCM**).
- Documento Maestro **v5.2** (orquestador `app_manager_aq`).
- Documento de Componente: **Component\_USB-NCM v2.0** (RPi=DHCP server, ESP32=DHCP client).
- Enfoque aprobado: **callbacks v0.18+** (`tud_mount_cb`/`tud_umount_cb`), `` en mount/unmount, `` consistente.

## 3) Stack / Versiones

- **ESP‑IDF**: v5.5
- **esp\_tinyusb**: ≥ 1.7.0
- **TinyUSB**: ≥ 0.18 (callbacks actualizados; no `tud_network_link_state_cb`).

## 4) Objetivo funcional

- Crear un componente que:
  1. **Instale** TinyUSB y la clase **tinyusb\_net** (NCM por defecto, **ECM** como fallback opcional).
  2. **Cree** un `esp_netif` **ETH-like** y lo **adjunte** al glue de `tinyusb_net`.
  3. **Arranque** el **cliente DHCP** al montar USB (y lo detenga al desmontar).
  4. Exponga **API mínima estable** y **eventos** básicos (GOT\_IP / LOST\_IP) para consumo del orquestador.

## 5) Entregables / Archivos (en `/components/usb_netif_aq/`)

1. `` — API pública:

```c
#pragma once
#include "esp_netif.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t  mac_addr[6];    // Debe coincidir con iMacAddress
    bool     use_ecm_fallback; // true=ECM, false=NCM
    const char *hostname;    // opcional; NULL para omitir
} usb_netif_cfg_aq_t;

esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg);
esp_err_t usb_netif_start_aq(void);     // tinyusb_driver_install + tinyusb_net_init + crear/attach esp_netif
esp_err_t usb_netif_stop_aq(void);
esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out);
bool      usb_netif_is_link_up_aq(void);

// Bloquea hasta GOT_IP o timeout; devuelve IP si se solicita
esp_err_t usb_netif_wait_got_ip_aq(TickType_t timeout, esp_ip4_addr_t *out_ip);
```

2. `` — implementación **esqueleto**:

   - **Prohibido**: `esp_netif_create_default_wifi_sta()`; usar `esp_netif_new(ESP_NETIF_DEFAULT_ETH())`.
   - **TinyUSB**: `tinyusb_driver_install()`; `tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg)` con **MAC** provista.
   - **Glue**: `esp_netif_attach(usb_netif, usb_netif_glue_from_tinyusb())` (función/objeto glue que proveas aquí como stub mínimo).
   - **Callbacks (TinyUSB ≥ 0.18)**:
     - `tud_mount_cb()`: marcar **link UP** y `esp_netif_dhcpc_start(netif)`; `ESP_LOGI` con resultado.
     - `tud_umount_cb()`: `esp_netif_dhcpc_stop(netif)` y marcar **link DOWN**.
   - **Eventos IP**: registrar `IP_EVENT_ETH_GOT_IP` / `IP_EVENT_ETH_LOST_IP`, solo logs + señal a `EventGroup`/semáforo para `wait_got_ip`.
   - **Logging**: TAG = `"usb_netif_aq"`.
   - **IRAM**: opcional en callbacks, sin trabajo pesado.

3. `` — opciones mínimas:

```
menu "usb_netif_aq"
config AQ_USB_USE_ECM_FALLBACK
    bool "Usar ECM como fallback"
    default n
config AQ_USB_HOSTNAME
    string "Hostname USB"
    default "esp32-usbncm"
config AQ_USB_LOG_LEVEL
    int "Log level (0-5)"
    range 0 5
    default 3
endmenu
```

4. `` — dependencias/flags:

```
idf_component_register(
    SRCS "usb_netif_aq.c"
    INCLUDE_DIRS "."
    REQUIRES esp_tinyusb esp_netif nvs_flash esp_event driver
)
```

5. `` — pinning recomendado:

```
dependencies:
  espressif/esp_tinyusb:
    version: ">=1.7.0"
  # TinyUSB >= 0.18 ya viene referenciado por esp_tinyusb/IDF; si se usa submodule, fijar tag >= 0.18
```

## 6) Reglas de diseño (obligatorias)

- **MAC estable** desde eFuse (`esp_efuse_mac_get_default()`), aplicando **LAA/unicast** sin romper unicidad:
  - `mac[0] |= 0x02; mac[0] &= ~0x01;`
- **Misma MAC** en `tinyusb_net_init()` y en el descriptor `` (12 HEX sin `:`).
- **DHCP**: únicamente **cliente** en ESP32 (no `esp_netif_dhcps_start()` aquí).
- **Callbacks**: usar **solo** `tud_mount_cb` / `tud_umount_cb` (no `tud_network_link_state_cb`).
- **No Wi‑Fi** en este componente. **ETH-like** únicamente.
- **Sin colas RX/TX** ni paths críticos todavía (esqueleto).

## 7) Estado / Eventos

- Estados lógicos: `USB_DOWN` → `USB_UP_NOIP` (mount + dhcpc) → `USB_UP_IP` (GOT\_IP) → `USB_RELEASING` (umount).
- `usb_netif_wait_got_ip_aq(timeout, &ip)` debe devolver `ESP_OK` o `ESP_ERR_TIMEOUT`.

## 8) Logs esperados (smoke)

- Mount: `"USB mounted, starting DHCP client"` + resultado `esp_err_to_name(ret)`.
- GOT\_IP: `"GOT_IP %s"` (imprimir `ip4addr_ntoa`).
- Umount: `"DHCP client stop result: %s"`.

## 9) Criterios de aceptación

- Compila en **ESP‑IDF v5.5** sin errores ni warnings relevantes.
- API en `usb_netif_aq.h` coincide con lo arriba definido.
- No hay referencias a `app_manager_aq` ni `mqtt_service_aq`.
- No se usa ninguna API **Wi‑Fi**.
- Callbacks correctos de **TinyUSB ≥ 0.18**.

## 10) Notas de implementación

- `hostname`: si no es `NULL`, aplicar `esp_netif_set_hostname(usb_netif, hostname)`.
- **Fallback ECM**: respetar `use_ecm_fallback` para elegir clase NCM o ECM en `tinyusb_net_init`.
- **Descriptor**: declarar `iMacAddress` (UTF‑16) alineado a `mac_addr` (ver doc de componente v2.0).

## 11) Entrega (PR)

- Rama sugerida: `feat/usb_netif_aq-skeleton-ncm-dhcpc`.
- Título PR: `feat(usb_netif_aq): skeleton NCM/ECM + DHCP client (TinyUSB v0.18+)`.
- Ubicación: `https://github.com/rolivape/esp32s3_firmaware`.
- Incluir breve README en el componente (opcional) con **cómo ver logs** y **smoke test**.

---

### Apéndice A — Esqueleto de callbacks (referencia)

```c
void tud_mount_cb(void) {
    // marcar link up + dhcpc_start
    ESP_LOGI("usb_netif_aq", "USB mounted, starting DHCP client");
    esp_netif_t *netif = /* obtener handle interno */;
    if (netif) {
        esp_err_t ret = esp_netif_dhcpc_start(netif);
        ESP_LOGI("usb_netif_aq", "DHCP client start result: %s", esp_err_to_name(ret));
    }
}

void tud_umount_cb(void) {
    // dhcpc_stop + link down
    esp_netif_t *netif = /* handle */;
    if (netif) {
        esp_err_t ret = esp_netif_dhcpc_stop(netif);
        ESP_LOGI("usb_netif_aq", "DHCP client stop result: %s", esp_err_to_name(ret));
    }
}
```

### Apéndice B — Generación de MAC/iMacAddress

```c
uint8_t mac6[6];
esp_efuse_mac_get_default(mac6);
mac6[0] |= 0x02;   // LAA
mac6[0] &= ~0x01;  // unicast
char mac_str[13];
snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
         mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);
// Usar mac6 en tinyusb_net_init y mac_str como iMacAddress en el descriptor.
```

