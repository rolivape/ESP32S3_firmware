# Prompt de Cambio — Corregir crash en `esp_netif_attach()` y actualizar Glue `esp_netif` ↔ TinyUSB

> Objetivo: eliminar el bucle y el **Guru Meditation (LoadProhibited)** en `esp_netif_attach()` implementando un **driver\_ifconfig** real (no-NULL) y corrigiendo el orden/uso del glue. Además, alinear la documentación **Component\_USB-NCM v2** con este enfoque.

---

## 1) Alcance

- Proyecto: **AquaControllerUSB** / ESP32-S3 / ESP-IDF v5.5.
- Componentes: `usb_netif_aq` (wrapper TinyUSB + netif), `usb_comms_aq` (fachada), `app_manager_aq` (orquestador).
- Host: RPi como **DHCP server**; ESP32 como **DHCP client** por USB (NCM preferido; ECM fallback).

## 2) Problema a corregir

Crash en `esp_netif_attach()` con `EXCVADDR: 0x00000000` ⇒ se está pasando **puntero NULL** (o estructura corrupta) como `driver_ifconfig`. **Causa raíz:** uso de función inexistente/placeholder (`usb_netif_glue_from_tinyusb()`), o `ifconfig` no inicializado.

## 3) Cambios en código (componentes)

### 3.1 `components/usb_netif_aq/src/usb_netif_aq.c`

**Añadir** el glue mínimo compilable y usarlo en `esp_netif_attach(...)`:

```c
#include "esp_netif.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

typedef struct {
    esp_netif_t *netif;
} usb_netif_drv_t;

static usb_netif_drv_t s_drv = {0};

// TX: de esp_netif → USB
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len) {
    (void)h;
    return (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(200)) == ESP_OK)
           ? ESP_OK : ESP_FAIL;
}

// RX: de USB → esp_netif
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx) {
    usb_netif_drv_t *drv = (usb_netif_drv_t*)ctx;
    if (drv && drv->netif) {
        esp_netif_receive(drv->netif, buffer, len, NULL);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void usb_netif_free_rx(void *h, void *buffer) { (void)h; (void)buffer; }

static esp_err_t usb_netif_post_attach(esp_netif_t *netif, void *args) {
    (void)args;
    s_drv.netif = netif;
    return ESP_OK;
}

static esp_netif_driver_ifconfig_t s_ifcfg = {
    .handle = &s_drv,
    .transmit = usb_netif_transmit,
    .driver_free_rx_buffer = usb_netif_free_rx,
    .post_attach = usb_netif_post_attach,
};
```

**Dentro de **``** (orden sugerido y logs):**

```c
esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
esp_netif_t *usb_netif = esp_netif_new(&cfg);
ESP_RETURN_ON_FALSE(usb_netif, ESP_FAIL, TAG, "esp_netif_new failed");

ESP_LOGI(TAG, "attach ifcfg=%p", (void*)&s_ifcfg);
ESP_RETURN_ON_ERROR(esp_netif_attach(usb_netif, &s_ifcfg), TAG, "attach failed");

// Hostname opcional
if (cfg_aq->hostname) esp_netif_set_hostname(usb_netif, cfg_aq->hostname);

// TinyUSB core + NET class
const tinyusb_config_t tusb_cfg = { .external_phy = false };
ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "tusb install failed");

static tinyusb_net_config_t net_cfg = {0};
memcpy(net_cfg.mac_addr, cfg_aq->mac_addr, 6);
net_cfg.on_recv_callback = usb_recv_callback;
net_cfg.free_tx_buffer   = NULL;
net_cfg.user_context     = &s_drv;
ESP_RETURN_ON_ERROR(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg), TAG, "net init failed");
```

**Callbacks de montaje (DHCP cliente en mount/unmount):**

```c
void tud_mount_cb(void) {
    if (s_drv.netif) {
        ESP_LOGI(TAG, "USB mounted, starting DHCP client");
        ESP_LOGI(TAG, "dhcpc_start: %s", esp_err_to_name(esp_netif_dhcpc_start(s_drv.netif)));
    }
}

void tud_umount_cb(void) {
    if (s_drv.netif) {
        ESP_LOGI(TAG, "dhcpc_stop: %s", esp_err_to_name(esp_netif_dhcpc_stop(s_drv.netif)));
    }
}
```

> **Nota:** No usar `tud_network_link_state_cb` (TinyUSB ≥ 0.18). Mantener este patrón.

### 3.2 `components/usb_netif_aq/include/usb_netif_aq.h`

- Sin cambios de API si ya contiene `usb_netif_install_aq/start/stop/get/is_link_up`.
- Asegurar comentarios que indiquen: **DHCP = cliente**; **no Wi‑Fi** aquí.

### 3.3 `components/usb_comms_aq/src/usb_comms_aq.c`

- **No** incluir headers de TinyUSB.
- Llamar a `usb_netif_install_aq(...)`, `usb_netif_start_aq()`, y proveer `usb_comms_wait_link_aq()` que espere `IP_EVENT_ETH_GOT_IP` (o EventGroup) y devuelva la IP.

### 3.4 Descriptores USB mínimos

- Añadir `` (12 HEX sin `:`) y **usar la MISMA MAC** en `net_cfg.mac_addr`.
- Mantener VID/PID y strings básicas. Esto elimina los warnings:
  - "No Device descriptor provided, using default".
  - "No String descriptors provided, using default".

### 3.5 Generación de MAC estable (LAA + unicast)

```c
uint8_t mac6[6];
esp_efuse_mac_get_default(mac6);
mac6[0] |= 0x02; // LAA
mac6[0] &= ~0x01; // unicast
char mac_str[13];
snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
         mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);
// mac6 → tinyusb_net_init(); mac_str → iMacAddress
```

## 4) Cambios en documentación (Component\_USB-NCM v2)

- **Sección 9 (Integración **``**)**:
  - **Eliminar** mención a `usb_netif_glue_from_tinyusb()` (no existe).
  - **Incluir** el patrón con `esp_netif_driver_ifconfig_t` (código de 3.1) y el `esp_netif_attach(…, &s_ifcfg)`.
- **Sección 12 (Troubleshooting)**:
  - Agregar punto: *"Crash en **`esp_netif_attach`**: verificar que **`&s_ifcfg`** no sea NULL, y que **`sizeof(esp_netif_driver_ifconfig_t)`** corresponda a IDF v5.5"*.
- **Sección 8 (iMacAddress/MAC)**: mantener la regla de unicidad y coherencia MAC.

## 5) Aceptación

- Firmware ya **no** entra en bucle; logs muestran:
  - `attach ifcfg=0x...` (puntero no-NULL)
  - `TinyUSB Driver installed`
  - `USB mounted, starting DHCP client`
  - `GOT_IP x.y.z.w` en < 8 s (configurable)
- Sin warnings de descriptores por defecto.
- `main.c` → `app_manager_aq` → `usb_comms_aq` → `usb_netif_aq` (sin TinyUSB fuera de `usb_netif_aq`).

## 6) Smoke test (RPi)

```bash
# Opción rápida con NetworkManager (DHCP+NAT)
sudo nmcli con add type ethernet ifname usb0 con-name usb0-shared \
  ipv4.method shared ipv4.addresses 192.168.7.1/24 ipv6.method ignore
sudo nmcli con up usb0-shared

# Ver DHCP
sudo tcpdump -i usb0 -n 'udp port 67 or 68' -c 6
ip neigh show dev usb0
```

## 7) Rama y PR

- Rama sugerida: `fix/usb_netif_glue-and-attach`
- Título PR: `fix(usb_netif_aq): real driver_ifconfig + stable attach; remove fake glue`
- Incluir diffs de: `usb_netif_aq.c`, descriptores USB, actualización de doc (sección 9 y 12).

