# Componente: USB-NCM para ESP32-S3 — **v2.0 (RPi = Servidor DHCP, ESP32 = Cliente DHCP)**

> **Arquitectura escandinava**: simple, funcional y explícita. Este documento redefine el diseño del componente USB-NCM para el proyecto AquaController-USB, migrando del enfoque “DHCP server en ESP32” a **“DHCP server en RPi (MASTER) y DHCP client en ESP32-S3 por USB-NCM”**.

---

## 1. Objetivo

Establecer un **enlace IP estable sobre USB** entre cada **ESP32-S3** y el **MASTER (RPi)** usando **CDC-NCM/ECM + tinyusb\_net**, donde:

- **RPi** asigna IPs (servidor DHCP) y, opcionalmente, NAT/route.
- **ESP32-S3** solicita IP vía **DHCP cliente** en su interfaz USB.
- El **canal USB** es el **transporte único** para MQTT en el Sprint 1 (sin Wi‑Fi).

## 2. Alcance

- Firmware ESP-IDF **v5.5** objetivo **ESP32‑S3**.
- **tinyusb v0.18+** (cambios de callbacks respecto a versiones previas).
- Componentes del sprint: `usb_netif_aq` (wrapper TinyUSB + netif), `usb_comms_aq` (inicializador), `mqtt_service_aq`, `app_manager_aq` (orquestador).
- **Host MASTER (RPi)**: configuración de DHCP en `usb0` (NetworkManager ó dnsmasq).

## 3. Diferencias clave vs diseño anterior

1. **DHCP**: se mueve al **MASTER**. El ESP32 **no** corre `dhcps`.
2. **Estado de enlace**: callbacks `tud_mount_cb()`/`tud_umount_cb()` reemplazan patrones antiguos (`tud_network_link_state_cb` deprecado). Estos callbacks **suben/bajan** el `esp_netif` y arrancan/paran `dhcpc`.
3. **Descriptor USB**: `iMacAddress` **obligatoria** (12 hex, sin “:”), consistente con la MAC usada por `tinyusb_net_init()`.

## 4. Requisitos

- **ESP-IDF 5.5** con `esp_netif`, `esp_event`, `nvs`.
- **tinyusb** integrado por **esp\_tinyusb**.
- **tinyusb\_net** (clase NCM/ECM) disponible.
- Convención de nombres `` (oficial del proyecto).

## 5. Arquitectura de componentes

### 5.1 `app_manager_aq` (Orquestador)

-

Responsable de la **secuencia de arranque** y la **coordinación**.

- Exporta: `app_manager_start()`.
- Llama a: `usb_comms_init_aq()` y luego a `mqtt_service_start_aq()` cuando haya IP.

### 5.2 `usb_comms_aq` (Inicializador de comunicaciones)

- Define el **flujo de inicialización** del stack USB-NCM **sin** incluir TinyUSB directamente.
- Exporta:
  - `esp_err_t usb_comms_init_aq(void);`
  - `esp_err_t usb_comms_wait_link_aq(TickType_t timeout, esp_ip4_addr_t *got_ip);`
- Internamente **invoca** funciones públicas de `usb_netif_aq` para instalar driver, crear netif, etc.

### 5.3 `usb_netif_aq` (Wrapper TinyUSB + Glue `esp_netif`)

- Encapsula **TinyUSB** (device) y la clase **tinyusb\_net** (NCM/ECM).
- Expone una **API mínima** y estable:
  ```c
  typedef struct {
      uint8_t mac_addr[6];       // MAC usada tanto en descriptor (iMacAddress) como en tinyusb_net_init
      bool use_ecm_fallback;     // true -> ECM, false -> NCM
      const char *hostname;      // opcional
  } usb_netif_cfg_aq_t;

  esp_err_t usb_netif_install_aq(const usb_netif_cfg_aq_t *cfg);
  esp_err_t usb_netif_start_aq(void);   // tinyusb_driver_install + tinyusb_net_init
  esp_err_t usb_netif_stop_aq(void);
  esp_err_t usb_netif_get_esp_netif_aq(esp_netif_t **out);
  bool      usb_netif_is_link_up_aq(void);
  ```
- **Callbacks**: implementa `tud_mount_cb()` y `tud_umount_cb()` para reflejar **link UP/DOWN** y notificar al `esp_netif` (arranque/parada de `dhcpc`).

## 6. Estado y máquina de estados (ESP32)

Estados del enlace USB (nivel lógico del componente):

- **USB\_DOWN**: cable desconectado o clase no montada. `dhcpc` parado.
- **USB\_UP\_NOIP**: `tud_mount_cb()` recibido, `dhcpc_start()` lanzado, esperando lease.
- **USB\_UP\_IP**: lease DHCP recibido (`IP_EVENT_ETH_GOT_IP`), IP válida disponible.
- **USB\_RELEASING**: transición al bajar enlace o al parar pila.

Eventos relevantes:

- `tud_mount_cb()` → `USB_UP_NOIP` + `esp_netif_dhcpc_start()`.
- `tud_umount_cb()` → `USB_DOWN` + `esp_netif_dhcpc_stop()`.
- `IP_EVENT_ETH_GOT_IP` → `USB_UP_IP` (notifica a `app_manager_aq`).
- `IP_EVENT_ETH_LOST_IP` → `USB_UP_NOIP`.

## 7. Secuencia de inicialización (ESP32)

1. `nvs_flash_init()`
2. `esp_event_loop_create_default()`
3. `usb_comms_init_aq()` → internamente:
   - `usb_netif_install_aq(cfg)` (set MAC/hostname, registra handlers IP\_EVENT).
   - `usb_netif_start_aq()`:
     - `tinyusb_driver_install()`
     - `tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg)` (NCM o ECM)
     - **crear** `esp_netif` tipo ethernet y **attach glue** tinyusb\_net ↔ esp\_netif.
4. Esperar `tud_mount_cb()` → `esp_netif_dhcpc_start()`.
5. Esperar `IP_EVENT_ETH_GOT_IP` → avisar a `app_manager_aq`.
6. `mqtt_service_aq` conecta al broker en RPi (por defecto `192.168.7.1:1883` o por DNS).

## 8. Descriptor USB — requisitos mínimos

- **iMacAddress**: cadena UTF-16 con **12 hex** (ej. `"02A1B2C3D4E5"`). No usar `:`.
- **Consistencia**: la misma MAC en `tinyusb_net_init()`.
- Verificación en host:
  ```bash
  lsusb -v -d 303a:4008 | grep -A5 -i mac
  ```

### 8.1 Generación de MAC estable (recomendado)

> Mantener unicidad por dispositivo y cumplir con LAA (Locally Administered Address) y unicast.

```c
// Obtener MAC base única desde eFuse (garantiza unicidad por chip)
uint8_t mac6[6];
esp_efuse_mac_get_default(mac6);

// Asegurar unicast y 'locally administered' sin perder unicidad
mac6[0] |= 0x02;   // LAA bit = 1 (locally administered)
mac6[0] &= ~0x01;  // Unicast (bit 0 = 0)

// String para iMacAddress (12 HEX, sin ':', en mayúsculas)
char mac_str[13];
snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
         mac6[0], mac6[1], mac6[2], mac6[3], mac6[4], mac6[5]);

// Usar la MISMA MAC:
// 1) tinyusb_net_init(): net_cfg.mac_addr = mac6;
// 2) Descriptor USB: iMacAddress = mac_str (UTF-16 en tabla de strings)
```

**Nota:** Evita `mac6[0] = 0x02;` ya que sobrescribe el OUI y puede romper la unicidad entre dispositivos. En su lugar, ajusta solo los bits LAA/unicast como arriba.

## 9. Integración `esp_netif` (ETH-like)

- Configuración base:
  ```c
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t *usb_netif = esp_netif_new(&cfg);
  // glue tinyusb_net <-> esp_netif
  esp_netif_attach(usb_netif, usb_netif_glue_from_tinyusb());
  if (cfg_aq->hostname) esp_netif_set_hostname(usb_netif, cfg_aq->hostname);
  // DHCP CLIENT se arranca solo cuando el USB sube (tud_mount_cb)
  ```
- Handlers de eventos a registrar en `usb_netif_install_aq`:
  - `IP_EVENT_ETH_GOT_IP` / `IP_EVENT_ETH_LOST_IP`.

## 10. Configuración del MASTER (RPi)

### Opción A — NetworkManager (rápida con NAT)

```bash
sudo nmcli con add type ethernet ifname usb0 con-name usb0-shared \
  ipv4.method shared ipv4.addresses 192.168.7.1/24 ipv6.method ignore
sudo nmcli con up usb0-shared
```

- Asigna **192.168.7.1/24** a la RPi y levanta **DHCP+NAT** para el ESP.

### Opción B — dnsmasq (control fino, sin NAT)

```bash
sudo apt-get install -y dnsmasq
sudo ip link set usb0 up
sudo ip addr add 192.168.7.1/24 dev usb0

# /etc/dnsmasq.d/usb0.conf
interface=usb0
bind-interfaces
except-interface=lo
except-interface=eth0
except-interface=wlan0
dhcp-range=192.168.7.50,192.168.7.150,255.255.255.0,12h
 dhcp-option=3,192.168.7.1
 dhcp-option=6,192.168.7.1
```

```bash
sudo systemctl restart dnsmasq
```

## 11. Pruebas y validación

**En RPi**

- Ver DHCP: `sudo tcpdump -i usb0 -n 'udp port 67 or 68'` (DISCOVER/OFFER/REQUEST/ACK).
- IP de peer: `ip neigh show dev usb0` (debería mostrar la MAC del ESP y su IP).

**En ESP32**

- Logs `IP_EVENT_ETH_GOT_IP` y dirección asignada.
- Ping al MASTER: `ping 192.168.7.1` (desde ESP si habilitado) o desde RPi a la IP del ESP.

**MQTT (Sprint 1)**

- Broker en RPi (ej.: Mosquitto en `192.168.7.1:1883`).
- `mqtt_service_aq` publica/subscribe por la interfaz USB.

## 12. Troubleshooting (rápido)

- ``**\*\*\*\*/dnsmasq no reparten IP**: confirmar que **solo hay un DHCP** en el segmento `usb0`.

- **Host log: \*\*\*\***``: falta o formato inválido en `iMacAddress`.

- **Link nunca sube**: callbacks antiguos; usar `tud_mount_cb()/tud_umount_cb()`.

- **Sin tráfico**: verificar `esp_netif_attach()` del glue tinyusb.

- **Multihost/Multi-ESP**: usar `usb1`, `usb2` … o reglas por MAC en dnsmasq.

- **MAC duplicada / pérdida de unicidad**: no sobrescribas `mac6[0] = 0x02` (pierde el OUI). Usa `mac6[0] |= 0x02; mac6[0] &= ~0x01;` para LAA unicast manteniendo el resto del eFuse.

## 13. Seguridad y aislamiento

- Crear VLAN/tabla de rutas en RPi si se desea aislar el segmento `192.168.7.0/24` del resto de la LAN.
- Aplicar **firewall** en RPi (solo puertos necesarios: MQTT, SSH de mantenimiento, etc.).

## 14. Compatibilidad ECM vs NCM

- **NCM** preferido por throughput;
- **ECM** como **fallback** para compatibilidad si el host exhibe inestabilidad con NCM.
- Exponer `use_ecm_fallback` en `usb_netif_cfg_aq_t` para conmutación de clase.

## 15. Versionado y pinning

- **tinyusb >= 0.18** (callbacks actualizados).
- Fijar versiones en `idf_component.yml`/`CMakeLists.txt` para reproducibilidad.

## 16. API pública mínima de cada componente

### 16.1 `usb_comms_aq`

```c
esp_err_t usb_comms_init_aq(void);
// Bloquea hasta tener IP o timeout; devuelve la IP obtenida
esp_err_t usb_comms_wait_link_aq(TickType_t timeout, esp_ip4_addr_t *got_ip);
```

- Internamente usa `usb_netif_aq` pero **no** incluye TinyUSB.

### 16.2 `usb_netif_aq`

(Ver sección 5.3). Debe exportar handle de `esp_netif` y estado link UP/DOWN.

## 17. Orden de implementación (Protocolo por Fases)

**Fase 1 – Orquestador (esqueleto):**

- Crear `app_manager_aq` con archivos `.c/.h/CMakeLists.txt` y funciones vacías (`app_manager_start()`).

**Fase 2 – Servicios aislados:**

- `usb_netif_aq`: implementar wrapper TinyUSB + glue `esp_netif` + callbacks mount/unmount + `dhcpc`.
- `usb_comms_aq`: inicialización de comunicaciones; expone `wait_link`.
- `mqtt_service_aq`: cliente MQTT apuntando a `192.168.7.1` (configurable).

**Fase 3 – Integración y refactor:**

- `app_manager_aq` llama a `usb_comms_init_aq()` → `wait_link` → `mqtt_service_start_aq()`.
- `main.c` queda mínimo: init sistema y `app_manager_start()`.

## 18. Criterios de aceptación (Sprint 1)

- RPi asigna IP al ESP32-S3 por `usb0` (DHCP OK en < 3 s).
- `ping` bidireccional RPi ↔ ESP.
- MQTT conecta y publica/consume en menos de 5 s desde link up.
- Reconexión tras desconectar/reconectar USB sin reiniciar firmware.

## 19. Snippets de referencia

**Descriptor – iMacAddress (ejemplo):**

```c
// "02A1B2C3D4E5" en UTF-16, declarado en tabla de strings y referenciado por el descriptor NCM/ECM
```

**Mount/Unmount → DHCPc (con logging):**

```c
void tud_mount_cb(void) {
    esp_netif_t *netif = usb_netif_get_handle_internal();
    if (netif) {
        ESP_LOGI(TAG, "USB mounted, starting DHCP client");
        esp_err_t ret = esp_netif_dhcpc_start(netif);
        ESP_LOGI(TAG, "DHCP client start result: %s", esp_err_to_name(ret));
    }
}

void tud_umount_cb(void) {
    esp_netif_t *netif = usb_netif_get_handle_internal();
    if (netif) {
        esp_err_t ret = esp_netif_dhcpc_stop(netif);
        ESP_LOGI(TAG, "DHCP client stop result: %s", esp_err_to_name(ret));
    }
}
```

## 20. Plan de pruebas automatizadas ( smoke )

- Script en RPi que, al detectar `usb0` **UP**, lanza `ping`, `mosquitto_pub/sub` y verifica latencia < 10 ms.
- `tcpdump` en `usb0` confirma **DISCOVER/OFFER/REQUEST/ACK** en cada reconexión.

---

### Anexo A — Guía rápida RPi

- **NetworkManager**: `nmcli con add ... ipv4.method shared ...`
- **dnsmasq**: archivo `usb0.conf` con `dhcp-range=192.168.7.50,192.168.7.150`.

### Anexo B — Logs esperados

- ESP32: `USB up, starting DHCP client…`, `GOT_IP 192.168.7.x`.
- RPi: `dnsmasq DHCPACK usb0 <MAC-ESP> 192.168.7.x`.

### Anexo C — Futuras extensiones

- Soporte multi-ESP (usb0/usb1/…): reglas por **MAC → IP fija** en dnsmasq.
- mDNS/LLMNR para descubrimiento por nombre (`hostname` del ESP).
- Métricas (LLDP/ethtool equivalente no aplica; usar counters del driver y `esp_netif`).

Mira esta recomendación que me hace, estas de acuerdo?\
Sí—la idea es sólida y estoy de acuerdo. Solo haría **4 ajustes clave** para que funcione a la primera:

1. \*\*No uses \*\*\`\`\
   Esa crea una interfaz Wi-Fi, no la de **USB-NCM/ECM**. Para USB debes crear un **netif tipo Ethernet** y “pegarlo” al glue de tinyusb:

```
c
```

CopyEdit

`// 1) TinyUSB primero ESP_ERROR_CHECK(tinyusb_driver_install(&(tinyusb_config_t){ .external_phy = false })); // 2) tinyusb_net (NCM o ECM) con MAC estable (misma que en iMacAddress del descriptor) tinyusb_net_config_t net_cfg = { 0 }; memcpy(net_cfg.mac_addr, mac6, 6); // mac6 = tu MAC única ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg)); // 3) Crear esp_netif tipo Ethernet y adjuntar glue tinyusb <-> esp_netif esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH(); esp_netif_t *usb_netif = esp_netif_new(&cfg); ESP_ERROR_CHECK(esp_netif_attach(usb_netif, usb_netif_glue_from_tinyusb())); // tu glue esp_netif_set_hostname(usb_netif, "esp32-usbncm-01"); // 4) Inicia DHCP **cuando el link USB esté UP** (tud_mount_cb) void tud_mount_cb(void) { esp_netif_dhcpc_start(usb_netif); } void tud_umount_cb(void) { esp_netif_dhcpc_stop(usb_netif); }`

2. **Descriptor USB con ****\`\`**** correcto**\
   Cadena de **12 hex sin “:”** (p. ej. `02A1B2C3D4E5`) y **coherente** con la MAC que pases a `tinyusb_net_init()`. Si no, en el host verás “failed to get mac address”.
3. **Callbacks actualizados en TinyUSB ≥0.18**\
   No uses `tud_network_link_state_cb` (ya no aplica). Usa `** / **` para subir/bajar el netif y arrancar/parar `dhcpc`.
4. **DHCP en la RPi: dnsmasq (control) o NM (fácil)**

- **dnsmasq** (tu propuesta, con un par de líneas extra):

```
bash
```

CopyEdit

`sudo apt install -y dnsmasq sudo ip addr add 192.168.100.1/24 dev usb0 # /etc/dnsmasq.d/usb0.conf interface=usb0 bind-interfaces dhcp-range=192.168.100.10,192.168.100.50,255.255.255.0,12h dhcp-option=3,192.168.100.1 # gateway dhcp-option=6,192.168.100.1 # DNS (opcional)`

```
bash
```

CopyEdit

`sudo systemctl restart dnsmasq`

- **NetworkManager (atajo con NAT y DHCP integrados):**

```
bash
```

CopyEdit

`sudo nmcli con add type ethernet ifname usb0 con-name usb0-shared \ ipv4.method shared ipv4.addresses 192.168.100.1/24 ipv6.method ignore sudo nmcli con up usb0-shared`

---

### Ventajas/consideraciones (confirmo lo tuyo)

- ✅ **Simplicidad en el ESP32** (solo `dhcpc`), mejor **debug** y **escalabilidad** en la RPi.
- ⚠️ Asegura **MACs estables** por dispositivo (descriptor + init).
- ⚠️ **Secuencia de arranque**: la RPi debe tener listo DHCP cuando conectes el ESP.
- 🔁 Si el host se pone quisquilloso con NCM, ofrece **ECM como fallback**.

