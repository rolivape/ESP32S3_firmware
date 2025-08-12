# usb_netif_aq Component

This component provides a USB network interface (NCM/ECM) for the ESP32-S3, acting as a DHCP client.

## How to Use

1.  Initialize the component:
    ```c
    usb_netif_cfg_aq_t cfg = {
        .use_ecm_fallback = false,
        .hostname = "my-esp32",
    };
    ESP_ERROR_CHECK(usb_netif_install_aq(&cfg));
    ```

2.  Start the component:
    ```c
    ESP_ERROR_CHECK(usb_netif_start_aq());
    ```

3.  Wait for an IP address:
    ```c
    esp_ip4_addr_t ip;
    if (usb_netif_wait_got_ip_aq(pdMS_TO_TICKS(10000), &ip) == ESP_OK) {
        // Got IP
    }
    ```

## Logging

To see the logs, run `idf.py monitor`. The component uses the tag `usb_netif_aq`.
You can configure the log level in `menuconfig` under `Component config > usb_netif_aq`.

## Smoke Test

1.  Connect the ESP32-S3 to a host (e.g., a Raspberry Pi).
2.  Configure the host to act as a DHCP server on the USB interface.
3.  The ESP32-S3 should connect and get an IP address.
4.  You should see "GOT_IP: ..." in the logs.
5.  You should be able to ping the ESP32-S3 from the host.
