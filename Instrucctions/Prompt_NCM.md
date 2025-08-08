Plantilla_Prompt.md - Instancia para Componente: usb_comms_aq (Integración DHCP Server via Kconfig y Wrappers Públicos)
Contexto del Proyecto y Referencias:

Documento Maestro v5.2: Enfatiza robustez en usb_comms_aq con DHCP server para IPv4 auto-asignación, configurado via Kconfig (CONFIG_LWIP_DHCPS=y). Anti-patrones: Evitar internals lwIP; priorizar wrappers esp_netif for build stability.
Component_USB-NCM.md v2.0: Tuning lwIP: Habilitar DHCP server via CONFIG_LWIP_DHCPS=y in Kconfig, llamar esp_netif_dhcps_start in code for host IP assignment (pool 192.168.7.100-254). Rendimiento: 1-6Mbps with coalescing.
Objetivos del Sprint 1: Integrar DHCP server for IPv4 functional, desbloqueando ping/MQTT before Fase 3.

Tarea Específica:
Refactoriza usb_comms_aq para habilitar DHCP server, usando Kconfig for feature (CONFIG_LWIP_DHCPS=y) and public wrappers esp_netif_dhcps_* in code, resolviendo build errors and enabling IPv4 auto in host.
Requisitos de Código y Estilo:

Kconfig Habilitación: Asegura CONFIG_LWIP_DHCPS=y in sdkconfig.defaults or Kconfig (add config LWIP_DHCPS bool "Enable DHCP server" default y).
Code in usb_netif_aq.c: After esp_netif_set_ip_info (static IP 192.168.7.1/24), add DHCP: #include "esp_netif.h"; esp_netif_dhcps_option_t opt_op = ESP_NETIF_OP_SET; esp_netif_dhcps_option_mode_t opt_mode = ESP_NETIF_DOMAIN_NAME_SERVER; dhcps_offer_t offer = DHCPS_OFFER_DNS; esp_netif_dhcps_option(netif, opt_op, opt_mode, &offer, sizeof(offer)); esp_netif_dhcps_start(netif). Remove any "lwip/dhcps.h" include.
Config Lease Optional: uint32_t lease_time = 120; esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_IP_ADDRESS_LEASE_TIME, &lease_time, sizeof(uint32_t)).
Logging: ESP_LOGI(TAG, "DHCP server started").
CMakeLists.txt: Maintain PRIV_REQUIRES lwip esp_netif esp_tinyusb; no INCLUDE_DIRS for lwIP (wrappers public).
Convención: Sufijo _aq. esp_err_t for errors (check esp_netif_dhcps_start).
Dependencias: idf_component.yml with espressif/esp_tinyusb.

Pruebas Requeridas:

Build: idf.py reconfigure && idf.py build sin errors.
Integración: Flashea; monitor logs with "DHCP server started", dmesg clean, dhclient usb0 in host assigns inet 192.168.7.x/24, ping 192.168.7.1 exitoso, iperf3 throughput 1-6Mbps.
CI: Matriz IDF 5.4/5.5.

Entregables:

Código in PR a rama feature/integrate-dhcp-usb-comms in https://github.com/rolivape/esp32s3_firmware.
README.md with note on Kconfig and wrappers, logs build/monitor/dmesg/ip addr pre/post.
Reporte métricas: IRAM usage