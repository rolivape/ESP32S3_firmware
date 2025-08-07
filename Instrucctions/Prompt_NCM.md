Plantilla_Prompt.md - Instancia para Componente: app_manager_aq (Refactorización Inicial)
Contexto del Proyecto y Referencias:

Documento Maestro v5.2: Reforzar la regla de que main.c debe ser mínimo y que toda la lógica de orquestación reside en app_manager_aq. Este componente es el "cerebro" que inicializa y gestiona los servicios.
Component_...md: Aún no existe un documento de diseño para app_manager_aq, pero su rol es el definido en el Documento Maestro: iniciar servicios, gestionar dependencias y manejar eventos del sistema.
Objetivos del Sprint 1: Esta tarea es un prerrequisito para continuar. Establece la estructura correcta del proyecto antes de añadir más funcionalidades o corregir otros bugs.

Tarea Específica: Refactoriza el código base actual para alinear la estructura con nuestra arquitectura. Esto implica dos acciones principales:

Crea el esqueleto del componente app_manager_aq. Debe estar ubicado en components/app_manager_aq/ e incluir CMakeLists.txt, idf_component.yml, include/app_manager_aq.h y src/app_manager_aq.c.
Mueve la lógica de main.c a app_manager_aq.c. La lógica de inicialización (creación del event loop, registro de manejadores de eventos) y el manejador de eventos (app_event_handler) deben ser transferidos al nuevo componente.
Limpia main.c. Después de la refactorización, main.c solo debe contener la inicialización de NVS (si es necesario) y la llamada a app_manager_start().

Requisitos de Código y Estilo:

Convención: Usa el sufijo _aq para todos los nombres de archivos y funciones públicas.
API Pública: El app_manager_aq.h debe exponer, por ahora, una única función: void app_manager_start(void);.
Dependencias: El idf_component.yml de app_manager_aq debe declarar una dependencia del componente usb_comms_aq.
Logging: Usa el TAG "APP_MANAGER_AQ" para todos los logs dentro de este componente.
Errores: Propaga los errores usando ESP_ERROR_CHECK() como se hacía en el main.c original.

Pruebas Requeridas:

Prueba de Regresión: La prueba principal es que, después de la refactorización, el proyecto compile (idf.py build) y se comporte exactamente igual que antes. La salida de dmesg y lsusb en el host debe ser idéntica.

Entregables:

Código completo en un PR a la rama feature/refactor-app-manager en https://github.com/rolivape/esp32s3_firmaware.
El PR debe incluir la creación del nuevo componente app_manager_aq y la modificación de main/main.c.
Un README.md básico para el nuevo componente.