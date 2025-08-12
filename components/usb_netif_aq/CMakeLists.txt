idf_component_register(
    SRCS
        "src/usb_netif_aq.c"
        "src/usb_descriptors_aq.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp_tinyusb nvs_flash esp_event efuse
    PRIV_REQUIRES driver esp_netif tinyusb
)
