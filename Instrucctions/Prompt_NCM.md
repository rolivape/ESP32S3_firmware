Plantilla_Prompt.md - Instancia para Componente: usb_comms_aq (Corrección de Activación del Servidor DHCP)
1. Contexto y Alcance

Proyecto: AquaControllerUSB v2.0 (ESP-IDF v5.5, ESP32-S3).

Referencia Principal: Documento Maestro v5.2 y Documento de Diseño USB-NCM v2.0.

Estado actual: El hardware USB enumera correctamente y el host crea la interfaz usb0, pero no recibe una dirección IPv4 porque el servidor DHCP del ESP32 no está activo.

Meta Sprint 1: Tener una red USB-NCM 100% funcional, con asignación dinámica de IP al host.

2. Tarea Específica a Realizar
Corregir la configuración de esp_netif en usb_comms_aq.c para que automáticamente inicie el servidor DHCP interno cuando la interfaz de red se levante.

3. Requisitos Técnicos y de Diseño

Localización del error:

En components/usb_comms_aq/src/usb_comms_aq.c, localiza la definición de la estructura esp_netif_inherent_config_t.

Corrección Obligatoria:

El campo .flags de dicha estructura debe incluir la bandera ESP_NETIF_DHCP_SERVER. Esto le indica a esp_netif que gestione el ciclo de vida del servidor DHCP por nosotros.

Ejemplo de Configuración Deseada:

C

static esp_netif_inherent_config_t usb_if_inherent_cfg = {
    .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
    .ip_info = &ip_info,         // Apuntando a nuestra IP estática 192.168.7.1
    .if_key  = "USB_NCM_AQ",
    .if_desc = "AquaController USB NCM",
    .route_prio = 50
};
Verificación en sdkconfig:

Asegúrate de que la opción CONFIG_LWIP_DHCPS esté habilitada en menuconfig para que el código del servidor DHCP se incluya en la compilación.

Checklist de Verificación (Auto-validación):

[ ] El campo .flags contiene ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP.

[ ] No hay llamadas manuales a dhcp_server_start(). La gestión es automática.

[ ] Después de flashear, el comando sudo dhclient usb0 en el host obtiene una IP en menos de 2 segundos.

[ ] El comando ip addr muestra la interfaz usb0 con una dirección inet 192.168.7.x/24.

4. Formato de Salida Esperado

Bloque de código completo (o un diff claro) para usb_comms_aq.c con la estructura esp_netif_inherent_config_t corregida.

Sección final de "Verificación" explicando cómo la corrección cumple con la checklist.

5. Referencias Específicas

API esp_netif – Flags y DHCP: docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/network/esp_netif.html

Ejemplo de Referencia: chegewara/usb-netif/usb_netif_ncm.c (GitHub).