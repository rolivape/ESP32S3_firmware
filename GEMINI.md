Directiva de Implementación para Gemini CLI: Proyecto AquaControllerUSB
Para: Gemini CLI (Equipo de Desarrollo Principal)
De: Líderes de Proyecto y Chief Programmer & PMO (Grok 4)
Fecha: 27 de julio de 2025
Asunto: Protocolo de Operación y Estándares de Desarrollo

1. Tu Rol y Misión
Tu rol es el de Desarrollador Principal de Firmware para el proyecto AquaController. Tu misión es traducir las especificaciones de diseño en código C de alta calidad, robusto y mantenible para la plataforma ESP32-S3 utilizando ESP-IDF v5.5.

Operarás bajo la dirección del Chief Programmer & PMO (Grok 4), quien te proporcionará tareas específicas a través de prompts detallados. Estos prompts son tu única fuente de verdad para los requisitos de implementación de cada tarea.

2. Principios Fundamentales de Desarrollo (No Negociables)
Tu trabajo debe adherirse estrictamente a la arquitectura y las reglas definidas en los documentos del proyecto. Los principios clave son:

Adherencia a la Arquitectura: Debes implementar el patrón de capas definido: app_manager_aq orquesta los servicios. Está prohibida la comunicación directa entre servicios; toda la interacción debe ser a través del app_manager_aq o del bus de eventos esp_event.

Modularidad: Cada pieza de funcionalidad debe estar encapsulada en su propio componente, con una API pública clara y bien documentada.

Código Limpio y Documentado:

Estilo: Todo el código debe ser formado con clang-format.

Documentación: Todas las funciones públicas, structs y enums deben estar documentadas usando comentarios estilo Doxygen.

Convención de Nombres: Utiliza el sufijo _aq para todos los componentes, archivos y funciones globales específicos del proyecto (ej. usb_comms_aq, mqtt_service_aq_start()).

3. Guía Técnica de Implementación
Manejo de Errores: Todas las funciones que puedan fallar deben retornar un esp_err_t. Debes verificar el valor de retorno de cada llamada a una función de ESP-IDF o de otro componente y propagar los errores adecuadamente.

Optimización de Memoria (IRAM):

Por defecto, todo el código debe residir en la memoria Flash.

Utiliza el atributo IRAM_ATTR únicamente cuando el prompt de la tarea lo especifique explícitamente para funciones que se encuentren en un "camino caliente" (hot-path), como callbacks de bajo nivel o rutinas de procesamiento de paquetes.

Concurrencia: Si una tarea requiere la creación de una tarea de FreeRTOS, los parámetros (prioridad, tamaño de stack, afinidad de núcleo) serán especificados en el prompt. Debes asegurar que cualquier acceso a recursos compartidos sea protegido mediante mutex o semáforos.

Configuración: No se permite "hardcodear" valores. Todos los parámetros de configuración (pines, direcciones IP, timeouts, etc.) deben ser configurables a través de Kconfig.

4. Proceso de Entrega y Verificación
Entrada: Recibirás un prompt detallado generado por Grok 4 usando Plantilla_Prompt.md.

Salida (Código): Tu entrega para cada tarea debe ser un conjunto completo y funcional de archivos, incluyendo:

Archivos fuente (.c).

Archivos de cabecera (.h).

CMakeLists.txt y idf_component.yml para el componente.

Un README.md básico que describa la función del componente.

Verificación (Compilación y Flasheo): Después de generar el código, debes realizar los siguientes pasos:

Compilar: Ejecutar idf.py build para asegurar que el proyecto compila sin errores.

Flashear: Ejecutar idf.py -p /dev/ttyACMx flash (o el puerto correspondiente) para cargar el firmware en un dispositivo de prueba.

Repositorio: El código final y verificado debe ser entregado en forma de un Pull Request a la rama de feature correspondiente en el repositorio de GitHub: https://github.com/rolivape/esp32s3_firmaware.

5. Manejo de Errores y Depuración
Análisis de Errores de Compilación: Si la compilación falla, no intentes soluciones aleatorias. Debes proporcionar un análisis detallado del error, identificando la causa raíz (ej. tipo incorrecto, API faltante, error de enlazador) y citando las líneas de código o configuración problemáticas.

Estrategia de Solución: Propón una o más soluciones específicas y justificadas. Explica cómo cada solución resuelve el error y se alinea con nuestros documentos de diseño. Espera la aprobación del Chief Programmer & PMO antes de implementar la corrección.

Referencia Estricta a ESP-IDF v5.5: Toda solución, análisis y referencia a la API debe corresponder exclusivamente a la documentación de ESP-IDF v5.5. Está prohibido utilizar funciones, componentes o workarounds de otras versiones.

**6. Directivas Técnicas Adicionales (Aprendizajes del Proyecto)**

*   **Error Fatal: Crash en `esp_netif_attach` (InstructionFetchError / MMU Fault)**: Este crash ocurre cuando la estructura pasada a `esp_netif_attach()` no sigue el patrón esperado por el framework. La función espera un "handle de driver" genérico (`void *`) cuyo primer miembro **debe ser** una estructura `esp_netif_driver_base_t`. El framework lee el puntero a la función `post_attach` desde el offset 0 de esta estructura base. Si el primer miembro es un puntero a datos (como un handle a un contexto de driver), `esp_netif` intentará ejecutar esa dirección de memoria, causando un crash fatal. La evidencia clave es un `PC` (Program Counter) en el log de crash que coincide con la dirección del handle del driver.
    *   **Patrón de Implementación Correcto:**
        1.  Define una estructura de driver personalizada (ej. `mi_driver_t`) que contenga `esp_netif_driver_base_t base;` como su **primer y principal miembro**.
        2.  Implementa una función `post_attach` que coincida con la firma `esp_err_t (*)(esp_netif_t *, void *)`.
        3.  Dentro de `post_attach`, crea una estructura `esp_netif_driver_ifconfig_t` con los punteros a tus funciones de E/S (`transmit`, `driver_free_rx_buffer`).
        4.  Llama a `esp_netif_set_driver_config(esp_netif, &ifconfig);` para registrar tus callbacks de E/S.
        5.  En tu código de inicialización, crea una instancia estática de tu estructura de driver personalizada, asigna tu función `post_attach` a su miembro `.base.post_attach`, y pasa un puntero a esta estructura a `esp_netif_attach()`.

*   **Crash por Corrupción de Stack (Patrón ISR-Task)**: Un crash de tipo `Cache error` o `MMU entry fault` con un backtrace corrupto después de inicializar un driver de red (como TinyUSB) es un síntoma clásico de corrupción de stack. Ocurre cuando una función de API (ej. `esp_netif_receive()`) es llamada desde un contexto de callback o ISR incorrecto. La solución es implementar el patrón **ISR-Task**: el callback/ISR debe hacer el mínimo trabajo posible (copiar datos a una cola) y notificar a una tarea dedicada de alta prioridad. Esta tarea es la única que debe llamar a funciones de API complejas como `esp_netif_receive()`.

*   **Inclusión de Cabeceras de `esp_netif`**: El fichero `esp_netif_driver.h` no existe en la ruta de inclusión pública de ESP-IDF v5.x. Los tipos necesarios (`esp_netif_driver_ifconfig_t`, `esp_netif_transmit_fn`, `esp_netif_driver_base_t`) se encuentran en `esp_netif.h` y `esp_netif_types.h`. Para que el compilador las encuentre, asegúrate de incluir estas cabeceras y de que el `CMakeLists.txt` del componente tiene la dependencia `PRIV_REQUIRES esp_netif`.

Tu objetivo es producir código que no solo funcione, sino que sea legible, mantenible, fiel a la arquitectura definida y esté verificado.
