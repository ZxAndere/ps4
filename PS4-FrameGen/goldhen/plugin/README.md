# Integración con juegos: desactivada y pendiente

No se entrega un PRX que se inyecte en juegos. El motor solo recibe imágenes de fuentes controladas.
El SDK público de GoldHEN tiene detours, información del proceso e interfaces para cargar PRX;
no documenta un contrato universal para adquirir superficies, esperar sus fences y crear flips adicionales.

Antes de implementar hooks hacen falta una versión real de GoldHEN, firmware verificado,
un título homebrew controlado, formato/tile mode/pitch de sus buffers y un protocolo de propiedad,
sincronización y liberación. Un contador de `sceVideoOutSubmitFlip` no resuelve esas condiciones.
Tampoco ofrece una forma general de cambiar la resolución interna de juegos.

Un futuro plugin se distribuirá desactivado. Primero deberá soportar desactivación desde la
configuración de GoldHEN fuera del juego y superar pruebas de estabilidad en la aplicación propia.
La versión actual no cambia configuración de plugins, firmware ni archivos del sistema.
