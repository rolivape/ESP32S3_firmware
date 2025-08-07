# Component build configuration for usb_comms_aq

idf_component_register(SRCS "src/usb_comms_aq.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_tinyusb esp_netif esp_event log lwip)
