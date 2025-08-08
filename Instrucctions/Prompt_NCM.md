Prompt para Gemini CLI
(formateado con la Plantilla_Promt.md e integrado al Documento Maestro v5.2 y al diseño USB-NCM v2.0)

1. Contexto y Alcance
Proyecto: AquaControllerUSB v2.0 (ESP-IDF v5.5, ESP32-S3).

Referencia Principal: Documento Maestro de Diseño y Arquitectura v5.2 y Documento de Diseño USB-NCM v2.0.

Estado actual:

usb_comms_aq ya crea la interfaz esp_netif con ESP_NETIF_DHCP_SERVER.

El servidor DHCP no se levanta porque la interfaz USB aún no se “sube” después de que el hardware reporta USB_NET_UP.

Objetivo Sprint 1: Red USB-NCM completamente funcional con DHCP operativo.

2. Tarea Específica a Realizar
Modificar app_manager_aq para que, al recibir el evento USB_NET_UP, ejecute esp_netif_up() sobre el netif de USB, iniciando así los servicios de red (incl. DHCP server).

3. Requisitos Técnicos y de Diseño
Localización del cambio:

Archivo: components/app_manager_aq/app_manager_aq.c

Función: event_handler_aq() (manejador registrado en el bus USB_NET_EVENTS).

Implementación obligatoria:


if (event_base == USB_NET_EVENTS && event_id == USB_NET_UP) {
    ESP_LOGI(TAG, "USB network is UP");
    esp_netif_t *netif = usb_comms_get_netif_handle();
    if (netif) {
        esp_netif_up(netif);   // ← inicia DHCP server y autoconfig
    }
}
Mantener arquitectura de capas:

Solo app_manager_aq orquesta; no lógica de red en servicios.

usb_comms_aq sigue siendo caja negra que publica USB_NET_UP/USB_NET_DOWN.

Checklist de auto-validación:

 esp_netif_up() llamado exclusivamente tras USB_NET_UP.

 No se llama dos veces si la interfaz ya está arriba (usar esp_netif_is_netif_up() si se desea).

 No rompe la secuencia de eventos existente.

4. Formato de Salida Esperado
Diff o bloque de código completo de app_manager_aq.c con la modificación.

Comentarios Doxygen explicando por qué se llama esp_netif_up().

Sección “Verificación” describiendo pruebas:

Conectar MCU → sudo dhclient usb0 debe finalizar < 2 s.

ip addr muestra usb0 inet 192.168.7.x/24.

Log del ESP32:


I APP_MANAGER_AQ: USB network is UP
I APP_MANAGER_AQ: esp_netif_up() called – DHCP server started
5. Referencias Específicas
API esp_netif_up() – IDF v5.5

Ejemplo de referencia: chegewara/usb-netif/usb_netif_ncm.c – llamada a esp_netif_up() tras tud_mount_cb.

