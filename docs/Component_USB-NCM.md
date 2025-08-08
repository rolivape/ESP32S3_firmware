Documento de Diseño y Arquitectura: Componente usb_comms_aq
Versión: 2.0
Fecha: 27 de julio de 2025
Estado: Aprobado para implementación

1. Filosofía de Diseño: De un Wrapper a un Driver de Bajo Nivel
La definición de usb_comms_aq ha evolucionado a partir de la experiencia de depuración y la necesidad de una robustez absoluta. Nuestra definición inicial era la de un "servicio de red" que actuaba como un wrapper sobre esp_tinyusb. Ahora, la definición evoluciona a la de un driver de dispositivo USB completo y autocontenido.

Antes: Delegábamos la complejidad de los descriptores USB al framework, esperando que menuconfig lo resolviera todo.

Ahora: Asumimos el control explícito y total sobre la capa de descriptores. El componente ya no es solo un consumidor de la API de TinyUSB, sino que se convierte en un proveedor de la "identidad" del dispositivo USB, implementando sus propios descriptores en usb_descriptors_aq.c. Esto es un aumento significativo en su responsabilidad, pero es la única forma de garantizar la robustez y compatibilidad que necesitamos.

2. Evolución de Responsabilidades
2.1. Gestión de la Dirección MAC: De Configurable a Única y Persistente
La definición del manejo de la dirección MAC también madura significativamente para garantizar la escalabilidad del sistema.

Antes: La MAC era un simple string en Kconfig, lo que introducía el riesgo de conflictos si se usaba el mismo firmware en múltiples dispositivos.

Ahora: La definición exige una solución "State of the Art". La MAC debe ser generada dinámicamente a partir del efuse único del chip, asegurando que cada panel tenga una identidad de red única sin intervención manual. El componente es ahora responsable de leer el hardware y construir su propia identidad persistente.

2.2. Configuración: De Implícita a Explícita
Este es el cambio filosófico más importante para asegurar la fiabilidad a largo plazo.

Antes: Confiábamos en la "magia" de la configuración de esp_tinyusb para que todo funcionara.

Ahora: Adoptamos un enfoque de control explícito. La implementación debe definir manualmente el string descriptor de la MAC, su formato (UTF-16LE) y su vinculación al descriptor NCM. Esto elimina las ambigüedades y nos protege de posibles cambios en el comportamiento por defecto del framework en futuras actualizaciones.

3. Conclusión: Alineación con los Principios "State of the Art"
Esta nueva y refinada definición del componente usb_comms_aq se alinea perfectamente con la filosofía de diseño establecida en el Documento Maestro:

Simplicidad Radical: Aunque la implementación interna es más compleja, la API pública sigue siendo engañosamente simple, ocultando la complejidad al resto del sistema.

Eficiencia Obsesiva: Al controlar los descriptores, podemos optimizar exactamente lo que el dispositivo anuncia, asegurando una comunicación eficiente.

Robustez Absoluta: Esta es la principal ganancia. Al no depender de comportamientos por defecto y definir explícitamente la identidad del dispositivo, creamos un componente mucho menos frágil y más predecible.

Esta implementación convertirá a usb_comms_aq en la base sólida que necesitamos para el resto del proyecto.