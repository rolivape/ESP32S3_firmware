Prompt para Gemini CLI: Tarea de Corrección Final del Sprint 1
(Formateado con la Plantilla_Prompt.md y alineado al Documento Maestro v5.2 y al diseño USB-NCM v2.0)

1. Contexto y Alcance
Proyecto: AquaControllerUSB v2.0 (ESP-IDF v5.5, ESP32-S3).

Referencia Principal: Documento Maestro de Diseño y Arquitectura v5.2 y Documento de Diseño USB-NCM v2.0.

Estado Actual:

El hardware USB (DWC2 + TinyUSB) ya enumera correctamente en el host.

El componente usb_comms_aq crea la interfaz esp_netif y el host la reconoce como usb0.

Problema: El host no recibe una dirección IP porque el servidor DHCP del ESP32 no se está iniciando.

Meta del Sprint 1: Tener una red USB-NCM 100% funcional, con asignación dinámica de IP al host.

2. Tarea Específica a Realizar
Corregir la configuración de esp_netif en usb_comms_aq.c para que arranque el servidor DHCP interno del ESP32 en la interfaz de red USB.

3. Requisitos Técnicos y de Diseño
Localización del Error:

En components/usb_comms_aq/src/usb_comms_aq.c, busca la instancia de la estructura esp_netif_inherent_config_t.

Corrección Obligatoria:

El campo .flags de dicha estructura debe incluir la bandera ESP_NETIF_DHCP_SERVER.

Ejemplo de Configuración Deseada:

// Ejemplo de la configuración correcta para la estructura inherente de esp_netif
static esp_netif_inherent_config_t usb_if_inherent_cfg = {
    .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
    .ip_info = &ip_info,         // Debe apuntar a la IP estática configurada (e.g., 192.168.7.1)
    .if_key  = "USB_NCM_AQ",
    .if_desc = "AquaController USB NCM",
    .route_prio = 50
};

Referencia de Buenas Prácticas:

Utiliza como guía el ejemplo chegewara/usb-netif/usb_netif_ncm.c, donde se activa el servidor DHCP con dicha bandera.

Checklist de Verificación (Auto-validación):

[ ] El campo .flags contiene ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP.

[ ] Después de flashear, el comando sudo dhclient usb0 en el host obtiene una dirección IP en menos de 2 segundos.

[ ] El comando ip addr en el host muestra la interfaz usb0 con una dirección inet 192.168.7.x/24.

4. Formato de Salida Esperado
Bloque de código completo (o un diff claro) para usb_comms_aq.c mostrando la estructura esp_netif_inherent_config_t corregida.

Comentarios explicativos in-line (estilo Doxygen).

Sección final de "Verificación" explicando cómo la corrección cumple con la checklist.

5. Referencias Específicas
API esp_netif – Flags y DHCP: docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/network/esp_netif.html

Ejemplo de Referencia: chegewara/usb-netif/usb_netif_ncm.c (GitHub).