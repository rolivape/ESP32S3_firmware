
### 1. Contexto y Alcance

- **Proyecto:** AquaControllerUSB_v2.0 (ESP-IDF v5.5, ESP32-S3).
- **Modelo de Arquitectura:** Modular por capas, con wrapper de red `usb_netif_aq` inicializado desde `usb_comms_aq`.
- **Stack de Comunicación:** USB-NCM sobre TinyUSB directo (no usar esp_tinyusb).
- **Objetivo:** Habilitar una interfaz de red NCM con MAC dinámica válida para el host Linux, garantizando que `cdc_ncm` no falle con "failed to get mac address / bind() failure".
- **Restricción crítica:** El descriptor debe incluir un campo `iMACAddress` válido que apunte a un string UTF-16LE de 12 caracteres hexadecimales (ej. `"A1B2C3D4E5F6"`), generado dinámicamente desde la MAC base del chip.

---

### 2. Tarea Específica

Implementar un módulo de red USB-NCM completo en ESP32-S3 con las siguientes características:

- Definir todos los **descriptores TinyUSB** manualmente (no usar esp_tinyusb).
- Implementar la función `tud_descriptor_string_cb()` para manejar strings dinámicos, en particular `iMACAddress`.
- Leer la MAC base del chip (`esp_read_mac()`), derivar una dirección USB local-administered válida, convertirla a ASCII hex (sin separadores) y a UTF-16LE.
- Incluir esta MAC como string descriptor en la tabla de descriptors y referenciar su índice en el descriptor CDC-NCM.
- Inicializar TinyUSB correctamente (`tusb_init()`), exponer interfaz NCM y permitir uso con `esp_netif`.

---

### 3. Estructura del Componente y Archivos Esperados

- `usb_netif_aq.c` → Contendrá la inicialización de TinyUSB y del driver de red USB (NCM).
- `usb_descriptors_aq.c` → Contendrá los descriptores USB completos, incluyendo `tud_descriptor_string_cb()`.
- `usb_netif_aq.h`, `usb_descriptors_aq.h` → Interfaces públicas.
- `CMakeLists.txt`, `idf_component.yml` → Configuración del componente.

---

### 4. Pseudocódigo Detallado

#### **usb_descriptors_aq.c**

```c
// Índices para string descriptors
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_MAC, // Este es iMACAddress
  STRID_MAX
};

static char mac_str_ascii[13]; // "A1B2C3D4E5F6"
static uint16_t _desc_str[32];

// Generar MAC desde chip
static void fill_mac_ascii_from_chip(void) {
    uint8_t base[6], mac[6];
    esp_read_mac(base, ESP_MAC_WIFI_STA);
    mac[0] = base[0] | 0x02;  // Local-administered
    mac[1] = base[1];
    mac[2] = base[2];
    mac[3] = base[3];
    mac[4] = base[4];
    mac[5] = base[5] ^ 0x55;
    snprintf(mac_str_ascii, sizeof(mac_str_ascii), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Callback requerido por TinyUSB
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    const char* str;
    switch (index) {
        case STRID_LANGID:
            _desc_str[1] = 0x0409; return _desc_str;
        case STRID_MAC:
            str = mac_str_ascii; break;
        // otros casos STRID_MANUFACTURER, etc.
    }

    // Convertir ASCII a UTF-16LE
    uint8_t len = strlen(str);
    for (uint8_t i = 0; i < len; i++) {
        _desc_str[1 + i] = str[i];
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return _desc_str;
}
```

#### **usb_netif_aq.c**

```c
void usb_netif_aq_start(void) {
    // Generar MAC primero
    fill_mac_ascii_from_chip();

    // Inicializar TinyUSB directamente
    tusb_init();

    // Crear esp_netif y asignar driver
    // (Este bloque depende del wrapper LWIP ↔ TinyUSB que ya tienes)
}
```

---

### 5. Requisitos Técnicos y Compatibilidad

- **TinyUSB API mínima**: `tusb_init()`, `tud_task()`, callbacks de descriptor.
- **Compatibilidad OS:** Linux, macOS y Windows 10+ (NCM).
- **No usar:** `esp_tinyusb` ni `descriptors_control.c`.
- **Sí usar:** ESP-IDF v5.5 (`esp_read_mac`, `esp_log`, `esp_netif`, etc.).
- **Licencia de TinyUSB**: BSD 3-Clause compatible.

---

### 6. Criterios de Validación

- [ ] El dispositivo se enumera correctamente como NCM.
- [ ] `dmesg` en Linux ya no muestra `failed to get mac address`.
- [ ] Aparece interfaz de red (`enx...`) al conectar.
- [ ] IP puede ser asignada por RPi host o estática (ej: 192.168.7.2).
- [ ] MAC generada es válida, local-administered, sin hardcode.

---

### 7. Instrucción Final

Genera todos los archivos necesarios (`usb_netif_aq.c/h`, `usb_descriptors_aq.c/h`) que implementen esta funcionalidad de forma modular, sin romper compatibilidad con el sistema de capas AquaController. El `main.c` solo debe llamar a `usb_comms_start()` o `usb_netif_aq_start()`.

Incluye comentarios explicativos donde haya lógica no trivial. Garantiza compatibilidad con ESP-IDF v5.5.
