idf_component_register(SRCS "src/usb_netif_aq.c" "src/usb_descriptors_aq.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__esp_tinyusb esp_netif usb
                    PRIV_REQUIRES lwip)

