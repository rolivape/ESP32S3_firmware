Documento de Diseño y Arquitectura: Aplicación Modular sobre USB-NCM
Versión: 2.0 (Final Merge & IRAM Strategy)
Fecha: 2025-08-05
Plataforma: ESP-IDF v5.5
Objetivo: Comunicación TCP/IP sobre USB nativo (ESP32-S3) mediante clase USB-NCM/ECM, con servicios modulares (MQTT, HTTP futuro), orquestados por app_manager_aq y desacoplados del medio físico mediante esp_netif.

1. Resumen Ejecutivo
Este documento define una arquitectura modular para habilitar red IP sobre USB en ESP32-S3 usando TinyUSB (clase NCM preferida; ECM y RNDIS como fallbacks opcionales). La lógica de aplicación reside en app_manager_aq, que arranca y detiene servicios especializados (p. ej. mqtt_service_aq) y gestiona cambios de interfaz (USB ↔ Wi-Fi) de forma explícita.

Cambios clave v2.0 (Merge)

Sincronización de API TinyUSB: Se sustituye tud_network_link_state_cb por tud_mount_cb/tud_umount_cb + tud_suspend_cb/tud_resume_cb (relevante para TinyUSB ≥ v0.18.0).

Gestión de Eventos: Se formaliza un bus de eventos USB_NET_EVENTS y una política de reconnect coordinada desde app_manager_aq.

Estrategia MQTT Robusta: Se adopta la estrategia de bind() del socket a la IP del esp_netif de USB para forzar la interfaz de red.

Optimización de Rendimiento y Memoria: Se incluyen recomendaciones específicas para el tuning de lwIP, colas de transmisión (TX), y una estrategia detallada para el uso eficiente de IRAM.

Pruebas y CI: Se define una matriz de pruebas multi-OS y una estrategia de Integración Continua para IDF 5.4/5.5.

Compatibilidad OS

Windows 10/11: NCM funciona de forma nativa. Windows <10 puede requerir un archivo .inf manual.

macOS y Linux: NCM/ECM funcionan de forma nativa. ECM puede ser preferible para compatibilidad con versiones antiguas de macOS.

Rendimiento Esperado: El puerto USB del ESP32-S3 es Full Speed (12 Mbps). El throughput real típico en NCM es de 1–6 Mbps, dependiendo del MTU, coalescing, nivel de logging y el tuning de lwIP.

2. Arquitectura del Sistema
2.1 Modelo de capas y flujo
+-----------------+
|    app_main.c   |  Punto de entrada mínimo
+-----------------+
        |
        v
+-----------------------------+
|       app_manager_aq        |  Orquestación y lógica de app
+-----------------------------+
      |                 |
      | (Inicia y        | (Inyecta dependencia
      |  obtiene netif)  |  y notifica eventos)
      v                 v
+-----------+     +-----------------+
| usb_comms |     | mqtt_service_aq |  Servicios especializados
|    _aq    |     +-----------------+
+-----------+

Capa inferior (app_main): inicialización mínima (NVS, esp_event_loop_create_default()) y delegación a app_manager_aq.

Capa intermedia (servicios): componentes caja negra que encapsulan una funcionalidad (USB-NCM, MQTT).

Capa superior (app_manager_aq): orquesta el arranque/parada ordenada, maneja errores y gestiona el cambio de interfaz de red (netif switching).

2.2 Concurrencia y escalabilidad
usb_comms_aq: debe correr en una tarea dedicada con prioridad 5 y un stack ≥ 4096 bytes para procesar datos de red sin latencia.

mqtt_service_aq: puede correr en una tarea separada con menor prioridad (ej. 3).

Pinning (Opcional): para aplicaciones sensibles al jitter, se puede fijar la tarea de usb_comms_aq a un núcleo específico (xTaskCreatePinnedToCore), pero se debe medir el impacto primero.

Múltiples netif: El app_manager_aq debe poder gestionar un cambio de interfaz (ej. app_manager_switch_netif(esp_netif_t*)). Este cambio debe publicar un evento NETIF_SWITCHED, y los servicios stateful (como MQTT) deben cerrar sus sockets y reconectar al recibirlo.

3. Diseño detallado de componentes
3.1 app_manager_aq
Responsabilidad: orquestar el ciclo de vida de la aplicación, la política de netif activa y la propagación de eventos y errores.

API pública (include/app_manager_aq.h)

#pragma once
#include "esp_netif.h"
void app_manager_start(void);
void app_manager_stop(void);
void app_manager_switch_netif(esp_netif_t* new_netif);

3.2 usb_comms_aq — Conectividad USB (NCM/ECM/RNDIS)
Responsabilidad: abstraer la complejidad del hardware USB y exponer un esp_netif estándar y funcional.

API pública (include/usb_comms_aq.h)

#pragma once
#include "esp_netif.h"
esp_err_t usb_comms_start(void);
esp_netif_t* usb_comms_get_netif_handle(void);

Manejo de eventos USB (TinyUSB ≥ v0.18.0)
Debe usar tud_mount_cb(), tud_umount_cb(), tud_suspend_cb() y tud_resume_cb() para detectar cambios de estado y emitir eventos USB_NET_* hacia app_manager_aq.

Rendimiento (TX/RX)

TX (NCM): usar tinyusb_net_send_async() con una cola (xQueueCreate) para gestionar el flujo (back-pressure), condicionando el envío a tud_ready().

RX: copiar siempre los datos entrantes a un búfer pbuf de LWIP antes de llamar a esp_netif_receive() para desacoplar los búferes de la pila USB.

3.3 mqtt_service_aq
Responsabilidad: cliente MQTT agnóstico al medio, con control explícito de la interfaz de red.

API pública (include/mqtt_service_aq.h)

#pragma once
#include "esp_netif.h"
// ... (definiciones de callback y struct de configuración)
esp_err_t mqtt_service_start(const mqtt_service_config_t *config);
// ... (otras funciones)

Selección de interfaz (enlace a esp_netif)
Dado que esp-mqtt no tiene una API pública estable para fijar el netif, la estrategia recomendada es vincular el socket a la IP local del esp_netif deseado antes de conectar.

// Dentro del transporte personalizado o antes de conectar
int sock = /* socket del transporte */;
int ifx  = esp_netif_get_netif_impl_index(config->netif_handle);
setsockopt(sock, IPPROTO_IP, IP_BOUND_IF, &ifx, sizeof(ifx));
// Opcionalmente, bind() explícito a la IP del netif
connect(sock, ...);

4. Estrategia de Optimización de IRAM 🚀
La IRAM (Instruction RAM) es un recurso crítico y escaso para el rendimiento. El código ejecutado desde IRAM evita la latencia de la caché de la memoria Flash, garantizando una ejecución determinista.

4.1 Uso Base de IRAM
Un proyecto "vacío" ya utiliza una porción significativa de IRAM. Esto se debe a que ESP-IDF coloca componentes críticos del sistema en esta memoria por defecto para asegurar la estabilidad y el rendimiento. Esto incluye:

El kernel de FreeRTOS.

Código de arranque y configuración del sistema.

Manejadores de interrupciones (ISRs).

Funciones de librería de bajo nivel.

El espacio libre reportado por idf.py size es el presupuesto real para las optimizaciones de la aplicación.

4.2 Estrategia de Optimización Recomendada
Medir Primero: Analizar el uso de memoria con idf.py size antes de optimizar.

Optimizar por Tamaño: En menuconfig, configurar Compiler options → Optimization Level a Optimize for size (-Os). Esto reduce el tamaño general del código con un impacto mínimo en la velocidad.

Uso Quirúrgico de IRAM_ATTR: Esta es la estrategia más eficiente. Se debe aplicar el atributo IRAM_ATTR solo a las funciones que se encuentran en el "camino caliente" (hot path) del procesamiento de datos y que son críticas para la latencia.

Candidatos Ideales en usb_comms_aq:

Los callbacks de TinyUSB: tud_mount_cb, tud_umount_cb, etc.

El callback de recepción de red: netif_recv_callback.

La función de transmisión de red: netif_transmit.

// En usb_comms_aq.c
#include "esp_attr.h"
void IRAM_ATTR netif_recv_callback(void *buffer, uint16_t len, void *ctx) {
    // ... Lógica crítica
}

Uso de Archivos de Fragmentos del Enlazador (Avanzado): Si se requiere más control, se puede usar un archivo linker.fragment en el CMakeLists.txt del componente para colocar funciones específicas o un componente entero en IRAM (iram0_0_seg) o en la SRAM principal mapeada como IRAM. Esta técnica debe usarse con cuidado, ya que la SRAM utilizada para instrucciones no estará disponible para datos (heap).

5. Flujo de datos
5.1 Diagrama de secuencia (TX asíncrona)
app_manager_aq -> mqtt_service_aq: publish("topic","data")
mqtt_service_aq -> lwIP: send()
lwIP -> esp_netif: .transmit(packet)
esp_netif -> usb_comms_aq: netif_transmit(packet)
usb_comms_aq -> TinyUSB: tinyusb_net_send_async(packet)
TinyUSB -> Host PC: [USB Data]

5.2 Manejo de fallos y optimización
Unplug durante TX: El evento USB_NET_DOWN debe provocar que se baje el esp_netif, se notifique al app_manager_aq y se prepare una reconexión futura.

Coalescing: Investigar la agrupación de paquetes en NCM para reducir la sobrecarga del USB.

6. Gestión de eventos, errores y logging
Bucle de eventos: Declarar bases de eventos (USB_NET_EVENTS, APP_EVENTS) y registrar manejadores con esp_event_handler_instance_register() para poder desregistrarlos de forma segura.

Errores: Propagar esp_err_t y permitir que app_manager_aq implemente políticas de reintento con exponential backoff.

Logging: Usar ESP_LOGx con TAG único. Evitar ESP_LOG_BUFFER_HEX en caminos de código críticos para el rendimiento.

7. Pruebas y robustez
Unit tests (Unity): Usar mocks de esp_netif_receive/transmit y simular los callbacks de TinyUSB (tud_mount_cb, etc.).

Integración: Probar con iperf3 y Wireshark en Windows, macOS y Linux. Los casos de prueba deben incluir desconexiones, suspensión del host y ráfagas de datos.

CI/CD: Usar GitHub Actions con una matriz para IDF {5.4, 5.5} y versiones fijadas de esp_tinyusb.

8. Configuración del proyecto
idf_component.yml (ejemplo)

dependencies:
  idf: ">=5.4.0,<6.0.0"
  espressif/esp_tinyusb: ">=1.7.0,<2.0.0"
  espressif/esp-mqtt: "~1.5"

Kconfig (extracto)

menu "USB Network Configuration"
    config USB_NET_CLASS
        string "USB Network Class"
        default "NCM"
        help
          Options: NCM, ECM, RNDIS

    config USB_NET_IPV4_ADDR
        string "Static IPv4 Address"
        default "192.168.7.1"
endmenu

9. Consideraciones futuras
Gestión de energía: En tud_suspend_cb, reducir la frecuencia del reloj para entrar en modos de bajo consumo.

Fallback de red: Implementar la lógica en app_manager_aq para priorizar USB y cambiar a Wi-Fi si USB_NET_DOWN es detectado.

Seguridad: Usar mTLS para MQTT/HTTP, con certificados cargados de forma segura desde NVS cifrada.

10. Referencias
ESP-IDF v5.5: Get Started, Changelog y Linker Script Generation.

esp_tinyusb: Guía de clases ECM/NCM/RNDIS y ejemplos.

TinyUSB: Release notes v0.18.0 (reescritura NCM y cambios en callbacks).

Ejemplo usb-netif con DHCP y logs.

Discusiones sobre socket binding a esp_netif (IP_BOUND_IF, esp_netif_get_netif_impl_index).

11. Apéndice: Parámetros y Notas Operativas
Rangos IP: Evitar 192.168.137.0/24 (ICS de Windows).

Tuning lwIP: Aumentar PBUF_POOL_SIZE si se observan paquetes perdidos (drops).

RNDIS: Usar solo por compatibilidad heredada; requiere un control de flujo (back-pressure) estricto en la transmisión.

Métricas: Implementar contadores para drops, retries, latencia de TX y profundidad de la cola de transmisión para monitorear el rendimiento.