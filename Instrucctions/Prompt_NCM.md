Plantilla_Prompt.md - Instancia para Componente: usb_comms_aq (Corrección DHCP Server para IPv4 Auto-Asignación)
Contexto del Proyecto y Referencias:

Documento Maestro v5.2: Enfatiza resiliencia en usb_comms_aq con IP auto-asignada via DHCP server para compatibilidad host. Anti-patrones: Evitar configs que requieran manual intervention; priorizar lwIP tuning para DHCP.
Component_USB-NCM.md v2.0: Rendimiento: Tuning lwIP (PBUF_POOL_SIZE), coalescing NCM; IP estática en device, pero DHCP server para host auto-config (evitar 192.168.137.0/24 conflicts).
Objetivos del Sprint 1: Corregir IPv4 en host para NCM funcional antes de MQTT. Enfoque en robustez sin sobrecarga IRAM.

Tarea Específica: Refactoriza usb_comms_aq para habilitar DHCP server en lwIP, permitiendo auto-asignación IPv4 al host (e.g., 192.168.7.100 from pool).
Requisitos de Código y Estilo:

En usb_comms_aq.c, después de esp_netif_attach y set IP estática (192.168.7.1), inicia DHCP server: #include "lwip/dhcps.h"; dhcps_offer_t offer = DHCPS_OFFER_DNS; dhcps_set_new_lease_cb(NULL); esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer, sizeof(offer)); esp_netif_dhcps_start(netif).
Config pool: esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(lease_time)); donde lease_time = 120 (mins).
Logging: ESP_LOGI(TAG, "DHCP server started on %s", esp_netif_get_ifkey(netif)).
sdkconfig: Habilitar via menuconfig: Component config > LWIP > DHCP server > Enable DHCP server (CONFIG_LWIP_DHCPS=y).
Mantén static IP en device: esp_netif_ip_info_t ip_info = { .ip = { .addr = IPADDR4_INIT_BYTES(192,168,7,1) }, .netmask = { .addr = IPADDR4_INIT_BYTES(255,255,255,0) }, .gw = { .addr = IPADDR4_INIT_BYTES(192,168,7,1) } }; esp_netif_set_ip_info(netif, &ip_info).
Convención: Sufijo _aq. esp_err_t para errores.
Dependencias: Asegura lwIP en idf_component.yml si necesario.

Pruebas Requeridas:

Build: idf.py build sin errors.
Integración: Flashea; verifica dmesg clean, ip addr en host muestra inet 192.168.7.x/24 auto (no manual), ping 192.168.7.1 exitoso, iperf3 throughput 1-6Mbps.
CI: Matriz IDF 5.4/5.5.

Entregables:

Código en PR a rama feature/fix-dhcp-usb-comms en https://github.com/rolivape/esp32s3_firmware.
README.md con nota de DHCP, logs dmesg/ip addr pre/post.
Reporte métricas: IRAM usage.