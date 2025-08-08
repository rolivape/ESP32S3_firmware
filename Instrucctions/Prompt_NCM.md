1. Contexto y Alcance

Proyecto: AquaControllerUSB v2.0 (ESP-IDF v5.5, ESP32-S3).
Referencia Principal: Documento Maestro v5.2 y Documento de Diseño USB-NCM v2.0.
Decisión Estratégica: Se abandona el enfoque de usar TinyUSB directamente. Se mantendrá la dependencia del componente oficial espressif/esp_tinyusb, pero se le inyectarán nuestros descriptores personalizados a través de su API de configuración. Esta es la solución final y canónica.
Meta: Lograr una base de firmware 100% estable que compile, se ejecute sin crashes y establezca una conexión de red USB-NCM funcional con el host.

2. Tarea Específica a Realizar Refactoriza el componente usb_comms_aq para integrar correctamente los descriptores USB personalizados con el componente esp_tinyusb, resolviendo el conflicto de "múltiples definiciones" y los fallos de inicialización.
3. Requisitos Técnicos y de Diseño

Estructura de Archivos:

Dentro de components/usb_comms_aq/src/, crea un nuevo archivo llamado usb_descriptors_aq.c.


Implementación en usb_descriptors_aq.c:

Define los arrays de descriptores USB completos como static const:

tusb_desc_device: El descriptor de dispositivo.
tusb_desc_configuration: El descriptor de configuración, que debe incluir la interfaz CDC-NCM y el descriptor funcional de Ethernet con el campo iMACAddress apuntando a un índice de string (ej. STRID_MAC).
tusb_string_descriptors: Un array de strings que incluya Fabricante, Producto, Número de Serie y el string de la MAC address.


Implementa la lógica para generar dinámicamente el string de la MAC a partir del efuse del chip (esp_read_mac()) y formatearlo como una cadena de 12 caracteres hexadecimales.
Crea una función de callback tud_descriptor_string_cb() que devuelva el string de la MAC (convertido a UTF-16LE) cuando el host lo solicite.


Refactorización en usb_comms_aq.c:

Elimina cualquier implementación de callbacks de bajo nivel (como tud_descriptor_string_cb, etc.) de este archivo. Su lugar es usb_descriptors_aq.c.
En la función de inicialización, antes de cualquier otra llamada, crea una instancia de tinyusb_config_t.
Puebla esta estructura con punteros a los descriptores definidos en usb_descriptors_aq.c:
text// En usb_comms_aq.c
extern const uint8_t tusb_desc_configuration[]; // Declarar como extern
// ...

tinyusb_config_t tusb_cfg = {
    .device_descriptor = NULL, // Opcional, puede usar el por defecto
    .string_descriptor = NULL, // Opcional, puede usar el por defecto
    .configuration_descriptor = tusb_desc_configuration, // ¡Este es el más importante!
    .external_phy = false
};

Llama a tinyusb_driver_install(&tusb_cfg), pasándole esta configuración.
Procede con la inicialización de la red como antes, llamando a tinyusb_net_init().


Verificación de Dependencias:

Asegúrate de que idf_component.yml tiene la dependencia de espressif/esp_tinyusb.
Asegúrate de que CMakeLists.txt añade el nuevo archivo src/usb_descriptors_aq.c a la lista de fuentes.



4. Criterios de Verificación (Auto-validación)

 El proyecto compila sin errores de "múltiples definiciones".
 El firmware se ejecuta sin el crash Descriptors config failed.
 La salida de dmesg en el host es limpia y muestra el registro del driver cdc_ncm.
 El comando ip addr en el host muestra la interfaz usb0 con una dirección IPv4 asignada por DHCP.
 El comando ping a la IP del ESP32 funciona correctamente.

5. Formato de Salida Esperado

Código completo para los archivos modificados (usb_comms_aq.c, CMakeLists.txt) y el nuevo archivo (usb_descriptors_aq.c).
Un diff claro en el PR para facilitar la revisión.
Sección final de "Verificación" con los logs que demuestren el éxito de la implementación.

6. Referencias Específicas

Ejemplo de Referencia: examples/peripherals/usb/device/tusb_custom_device (para ver cómo pasar descriptores a tinyusb_driver_install).
Documentación de tinyusb_config_t: docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_tinyusb.html