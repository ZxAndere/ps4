# Diagnóstico necesario en hardware

Registrar manualmente firmware y versión exacta de GoldHEN, modelo y modo de pantalla.
El identificador `GOLDHEN_SDK_VERSION` 1.00 del SDK no es la versión instalada de GoldHEN.
No conectar un binloader ni cargar plugins hasta establecer la compatibilidad.

La aplicación propia registra dimensiones y pitch de la superficie registrada y el modo de pantalla
devuelto por VideoOut por separado. Los FPS proceden de capturas reales y confirmaciones de flip.
El retardo de sondeo puede afectar esa medición; no mide latencia mando-a-fotón.
