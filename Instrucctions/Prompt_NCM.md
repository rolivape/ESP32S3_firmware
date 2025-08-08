lantilla_Prompt.md - Instancia for Componente: usb_comms_aq (Integración DHCP Server: Call in Code, Habilitado via Kconfig)
Contexto del Proyecto y Referencias:

Documento Maestro v5.2: Enfatiza robustez in usb_comms_aq with DHCP server for IPv4 auto, configurado via Kconfig (CONFIG_LWIP_DHCPS=y), called in code for start.
Component_USB-NCM.md v2.0: Tuning lwIP: Habilitar CONFIG_LWIP_DHCPS=y in Kconfig, call esp_netif_dhcps_start in code for host IP assignment (pool 192.168.7.100-254). Rendimiento: 1-6Mbps with coalescing.
Objetivos del Sprint 1: Integrar DHCP for IPv4 functional, desbloqueando ping/MQTT before Fase 3.

Tarea Específica:
Refactoriza usb_comms_aq for add call to esp_netif_dhcps_start in code, habilitado via Kconfig (CONFIG_LWIP_DHCPS=y), resolving IPv4 auto-asignación.
Requisitos de Código y Estilo:

Kconfig: Add in components/usb_comms_aq/Kconfig: config LWIP_DHCPS bool "Enable DHCP server" default y help "Enable DHCP server for USB NCM host auto-IP".
Code in usb_netif_aq.c: #include "esp_netif.h"; after esp_netif_set_ip_info (static IP 192.168.7.1/24), add #if CONFIG_LWIP_DHCPS esp_netif_dhcps_option_t opt_op = ESP_NETIF_OP_SET; esp_netif_dhcps_option_mode_t opt_mode = ESP_NETIF_DOMAIN_NAME_SERVER; dhcps_offer_t offer = DHCPS_OFFER_DNS; esp_netif_dhcps_option(netif, opt_op, opt_mode, &offer, sizeof(offer)); esp_netif_dhcps_start(netif); #endif
Config Lease Optional: uint32_t lease_time = 120; esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(uint32_t)).
sdkconfig: idf.py reconfigure to apply Kconfig; verify CONFIG_LWIP_DHCPS=y.
Logging: ESP_LOGI(TAG, "DHCP server started on %s", esp_netif_get_ifkey(netif)).
Convención: Sufijo _aq. esp_err_t for errors (check esp_netif_dhcps_start).
Dependencias: PRIV_REQUIRES lwip esp_netif.

Pruebas Requeridas:

Build: idf.py reconfigure && idf.py build sin errors.
Integración: Flashea; monitor with "DHCP server started", dmesg clean, dhclient usb0 assigns inet 192.168.7.x/24, ping 192.168.7.1 exitoso, iperf3 throughput 1-6Mbps.
CI: Matriz IDF 5.4/5.5.

Entregables:

Código in PR a rama feature/integrate-dhcp-kconfig-usb-comms in https://github.com/rolivape/esp32s3_firmware.
README.md with note on Kconfig and code call, logs build/monitor/dmesg/ip addr pre/post.
Reporte métricas: IRAM usage.