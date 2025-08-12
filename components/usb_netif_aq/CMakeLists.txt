idf_component_register(
    SRCS "src/usb_netif_aq.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_tinyusb esp_netif nvs_flash esp_event driver esp_eth efuse
)
