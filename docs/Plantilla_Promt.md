### **1. Contexto y Alcance**
- **Proyecto:** AquaControllerUSB_v2.0 (ESP-IDF v5.5, ESP32-S3).
- **Referencia Principal:** Documento de Diseño y Arquitectura v2.0.
- **Reglas de Diseño Clave:**
    - **Arquitectura:** Seguir estrictamente el modelo de capas (`app_manager_aq` > `servicios`).
    - **Comunicación:** Usar el bus de eventos (`esp_event`) para notificaciones entre componentes.
    - **IRAM:** Usar `IRAM_ATTR` de forma quirúrgica solo en funciones de *hot-path* (callbacks, procesamiento de bajo nivel).
- **Anti-Patrones a Evitar:**
    - Dependencias directas entre servicios (ej: `mqtt_service_aq` no debe conocer a `usb_comms_aq`).
    - Lógica de aplicación en `main.c`.
    - Bloqueos (`vTaskDelay` largos) en tareas de alta prioridad.

---

### **2. Tarea Específica a Realizar**
> *[Describe aquí la tarea de forma clara y concisa. Ejemplo: "Generar el componente `storage_aq` para gestionar la configuración del sistema en NVS."]*

---

### **3. Requisitos Técnicos y de Diseño**
1.  **Componente y Estructura:**
    - **Nombre:** `[nombre_componente_aq]`
    - **Ubicación:** `/components/[nombre_componente_aq]/`
    - **Archivos a generar:** `[nombre_componente_aq].c`, `include/[nombre_componente_aq].h`, `CMakeLists.txt`, `idf_component.yml`.
2.  **API Pública (`.h`):**
    - **Funciones:** `[Lista de funciones públicas, ej: `storage_aq_init()`, `storage_aq_save_wifi_creds()`]`.
    - **Documentación:** Comentarios Doxygen para todas las funciones, parámetros y valores de retorno.
3.  **Librerías y Dependencias:**
    - **Permitidas:** `[Lista de librerías de ESP-IDF, ej: `nvs_flash.h`, `esp_log.h`]`.
    - **Nuevas Dependencias:** `[Especificar si se añade algo al `idf_component.yml`]`.
4.  **Integración con `app_manager_aq`:**
    - **Llamada:** `[Indicar dónde y cómo `app_manager_aq` debe llamar al nuevo componente, ej: "Llamar a `storage_aq_init()` en `app_manager_start()` después de NVS."]`
5.  **Optimización y Rendimiento:**
    - **IRAM:** `[Especificar qué funciones, si alguna, deben ir en IRAM, ej: "Ninguna para este componente"]`.
    - **Memoria:** `[Especificar si se debe usar PSRAM o si hay consideraciones de heap, ej: "Evitar `malloc` grandes y frecuentes."]`

---

### **4. Criterios de Auto-Validación (Checklist del Diseñador)**
*Antes de finalizar, confirma explícitamente el cumplimiento de estos puntos:*
- [ ] **Alineación Arquitectónica:** ¿El código respeta el patrón de capas y la comunicación por eventos?
- [ ] **Manejo de Errores:** ¿Todas las funciones que retornan `esp_err_t` son verificadas?
- [ ] **Seguridad de Concurrencia:** ¿El código es seguro para ejecutarse en su tarea? ¿Se usan mutex/semáforos si se accede a recursos compartidos?
- [ ] **Optimización de IRAM:** ¿Se cumple la estrategia de IRAM definida?
- [ ] **Compatibilidad:** ¿El código es compatible con ESP-IDF v5.5 y las APIs mencionadas?

---

### **5. Formato de Salida Esperado**
1.  **Bloque de Código Completo:** Para cada archivo (`.c`, `.h`, `CMakeLists.txt`, etc.).
2.  **Comentarios Explicativos:** Dentro del código, explicando la lógica compleja.
3.  **Sección de Verificación:** Un pequeño párrafo final explicando cómo el código generado cumple con los Criterios de Auto-Validación.

---

### **6. Referencias Específicas**
> *[Añade aquí enlaces directos a la documentación de Espressif relevante para la tarea. Ejemplo: `https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/storage/nvs_flash.html`]*

---
**Instrucción Final:** Genera el código y la verificación.