Plantilla_Prompt.md - Instancia para Componente: usb_comms_aq (Corrección de Bug: MAC Address Persistente)
Contexto del Proyecto y Referencias:

Documento Maestro v5.2: Enfatiza resiliencia y configuración externa en usb_comms_aq, usando recursos del chip (e.g., eFuse) para valores únicos como MAC. Anti-patrones: Evitar hardcode; priorizar métodos nativos ESP-IDF para reproducibilidad.
Component_USB-NCM.md v2.0: Inicialización TinyUSB: Poblar .mac_addr en tinyusb_net_config_t con valor válido antes de tinyusb_net_init(). Recomienda IP estática; extender a MAC generada dinámicamente para evitar conflictos.
Objetivos del Sprint 1: Corregir bug persistente en usb_comms_aq para NCM funcional. Enfoque en compatibilidad Linux (RPi) y verificación via dmesg/ip addr, sin sobrecarga IRAM.

Tarea Específica: Corregir el bug persistente que causa "failed to get mac address" y "bind() failure" en usb_comms_aq.c.
Requisitos de Código y Estilo:

Causa Raíz: tinyusb_net_config_t no poblada con MAC válida; parseo previo (sscanf de Kconfig) falló o no se aplicó correctamente.
Solución: En la función de inicialización de usb_comms_aq.c, generar MAC usando esp_efuse_mac_get_default() para obtener la base del chip (uint8_t mac_addr[6]; esp_efuse_mac_get_default(mac_addr);). Hacerla locally administered seteando mac_addr[0] |= 0x02; (bit 1 a 1, bit 0 a 0). Asignar a .mac_addr de tinyusb_net_config_t antes de tinyusb_net_init().
Implementación: Remover dependencia en sscanf/Kconfig string para MAC (usar eFuse como fallback robusto). Si Kconfig existe, opcionalmente override, pero priorizar eFuse.
Referencia de Implementación: Guíate por ejemplos ESP-IDF (e.g., esp_netif_set_mac() en docs) y usb_stack.c de "chegewara" (generación de MAC de eFuse y ajuste para interfaces virtuales como NCM).
Convención: Sufijo _aq en nombres. Usar esp_err_t para chequeos (e.g., ESP_ERROR_CHECK en esp_efuse_mac_get_default).
Dependencias: Asegura espressif/esp_tinyusb en idf_component.yml; incluye <esp_efuse.h> si necesario.
Logging: Agrega ESP_LOGI con TAG "USB_COMMS_AQ" para imprimir MAC generada (e.g., ESP_LOGI(TAG, "Generated MAC: %02x:%02x:...");) para depuración, pero evita en hot-path.
Errores/Resiliencia: Si eFuse falla, fallback a MAC default estática (e.g., {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}) y log error.

Pruebas Requeridas:

Regresión: Compila (idf.py build) y flashea; verifica no cambios no intencionales.
Integración: En RPi, confirma en dmesg que errores de MAC/bind no aparecen. Ejecuta ip addr show para interfaz USB activa con IP; prueba ping al ESP32 (192.168.7.1).
CI: Matriz IDF 5.4/5.5; agrega test para MAC válida en unit tests (Unity).

Entregables:

Código actualizado en PR a rama feature/fix-mac-address-usb-comms-v2 en https://github.com/rolivape/esp32s3_firmware.
PR solo cambios en usb_comms_aq (c/h); actualiza README.md con nota de corrección y ejemplo de MAC generada.
Reporte en PR: Logs dmesg/monitor pre/post, salida ip addr, throughput con iperf3.