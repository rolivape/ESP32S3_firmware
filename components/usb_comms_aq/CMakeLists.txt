idf_component_register(SRCS "src/usb_comms_aq.c"
                    INCLUDE_DIRS "include"
                    REQUIRES usb_netif_aq esp_event esp_netif
                    PRIV_REQUIRES lwip)

