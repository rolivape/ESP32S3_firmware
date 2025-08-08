Prompt para Gemini CLI
(formateado con la Plantilla_Promt.md y alineado al Documento Maestro v5.2 y al diseño USB-NCM v2.0)

1. Contexto y Alcance
Proyecto: AquaControllerUSB v2.0 (ESP-IDF v5.5, ESP32-S3).

Referencia Principal: Documento Maestro de Diseño y Arquitectura v5.2 y Documento de Diseño USB-NCM v2.0.

Estado actual:

El hardware USB (DWC2 + TinyUSB) ya enumera correctamente.

usb_comms_aq crea la interfaz esp_netif, pero el host no recibe IP porque el servidor DHCP del ESP32 no arranca.

Meta Sprint 1: Tener una red USB-NCM 100 % funcional, con asignación dinámica de IP al host.

2. Tarea Específica a Realizar
Corregir la configuración de esp_netif en usb_comms_aq.c para que arranque el servidor DHCP interno.

3. Requisitos Técnicos y de Diseño
Localización del error:

En components/usb_comms_aq/usb_comms_aq.c, busca la instancia de esp_netif_inherent_config_t (normalmente llamada s_inherent_cfg o similar).

Corrección obligatoria:

El campo .flags debe incluir ESP_NETIF_DHCP_SERVER y ESP_NETIF_FLAG_AUTOUP.

Ejemplo de configuración deseada:

c
Copy
Edit
static esp_netif_inherent_config_t usb_if_inherent_cfg = {
    .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
    .ip_info = &ip_info,         // 192.168.7.1/24
    .if_key  = "USB_NCM",
    .if_desc = "usb_ncm_netif",
    .route_prio = 30
};
Referencia de buenas prácticas:

Usar como guía el ejemplo chegewara/usb-netif/usb_netif_ncm.c, donde se activa el DHCP server con dicha bandera.

Mantener arquitectura:

No exponer TinyUSB fuera de usb_comms_aq.

Mantener la generación dinámica de la MAC y la tabla de strings.

Checklist de verificación (auto-validación):

 .flags contiene ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP.

 Llamada a esp_netif_create_default_wifi_* no sustituye la interfaz USB; sólo se usa esp_netif_new() con la config corregida.

 Después del flash, sudo dhclient usb0 en el host obtiene IP < 2 s.

 ip addr muestra usb0 con inet 192.168.7.x/24.

4. Formato de Salida Esperado
Bloque de código completo (o diff claro) para usb_comms_aq.c con la estructura esp_netif_inherent_config_t corregida.

Comentarios explicativos in-line (Doxygen).

Sección final “Verificación” explicando cómo la corrección cumple la checklist.

5. Referencias Específicas
API esp_netif – Flags y DHCP

Ejemplo de referencia: chegewara/usb-netif/usb_netif_ncm.c (GitHub).