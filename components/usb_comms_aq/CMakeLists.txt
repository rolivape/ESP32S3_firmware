idf_component_register(SRCS "src/usb_comms_aq.c" "src/tusb_descriptors_aq.c"
                        INCLUDE_DIRS "include"
                        REQUIRES espressif__esp_tinyusb esp_netif efuse)

