Contexto del Proyecto (Breve): AquaControllerUSB_v5.2 (ESP-IDF v5.5, ESP32-S3). Sigue documento maestro v5.2, mapa componentes/APIs, Reglas Diseño (Capítulo 9), Anexo 10.5 librerías válidas. Optimiza IRAM <80%, NVS configs, MQTT sobre USB-ECM, SAFE MODE autónomo. Anti-patrones: No nube/Wi-Fi crítico, no dependencias pesadas; fix DTR en ECM para estabilidad.
Tarea Específica: Refactoriza usb_netif_aq.c para activar red sin depender de tud_cdc_line_state_cb/DTR (usar tud_mount_cb o post-Set Config para tud_network_link_state_cb(true)). Integra con esp_netif, inicia DHCP/server si aplica, y soporta IPs estáticas via NVS.
Requisitos Clave (Lista numerada, max 5):

Librerías: esp_tinyusb (CDC-ECM), esp_netif, esp_dhcp_server (opcional); evita DTR dependency.
Diseño: /components/usb_comms_aq/, sufijo _aq, no hardcode (IPs en NVS), callbacks mínimos.
Optimizaciones: PSRAM buffers, reconexión <5s, IRAM <80% (CONFIG_SPI_FLASH_AUTO_SUSPEND).
APIs: tud_mount_cb, tud_network_link_state_cb, esp_netif_set_ip_info; callbacks para events up/down.
Integración/Testing: Llama desde main.c; stubs Unity para simular enumeración/host, probar en RPi5.

Auto-Validación Requerida:

Confirma cumplimiento IRAM (<80% via idf.py size conceptual: IRAM ~65KB usado, total 128KB), v5.5 compatibilidad (ECM estable sin DTR), y anti-patrones (estabilidad red sin bugs TinyUSB).
Simula output idf.py size (conceptual): IRAM: 70KB/128KB (55%), DRAM: 160KB/320KB, Flash: 1.3MB.

Salida Esperada:

Código C completo para usb_netif_aq.c y .h actualizados (#includes, funciones, error handling).
Comentarios detallados (explica remoción DTR dependency).
Verificación final: Explica cumplimiento (link-up post-enumeration, latencia MQTT <50ms).

Referencias: [docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_device.html], [github.com/hathach/tinyusb/discussions/742] (ECM impl), [docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/tinyusb_guide.html].
Genera y confirma. Subir commit a https://github.com/rolivape/aqua_controller/esp32_firmware/components/usb_comms_aq/.