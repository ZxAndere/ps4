# Protocolo pendiente de hardware

No se ha ejecutado ninguno de estos pasos en una PS4. Guardar cada resultado y fallo, incluso si el
instalador rechaza el paquete. No actualizar firmware para seguir esta lista.

1. Identificar versión completa de GoldHEN y su fuente; contrastar firmware 13.52 con sus notas oficiales.
   «Beta 12» por sí solo no permite hacerlo. Registrar PS4 Fat, conexión y modo de pantalla.
2. Solo tras resolver compatibilidad, instalar el PKG como aplicación homebrew propia y abrirla con motor OFF.
   Registrar errores de carga, dimensiones de superficie y modo físico consultado, sin extrapolar soporte.
3. Verificar escena original y abrir/cerrar el menú con L3+R3 al menos diez veces. Mantener ambos pulsados
   debe producir un solo cambio. Cambiar selección, cerrar con Círculo y salir con Options.
4. Activar ×2, ×3 y ×4 en perfil performance; repetir quality. Grabar salida con una capturadora si se dispone.
   Verificar frames distintos y orden temporal en la captura, no solo contadores o llamadas submit.
5. Para cada multiplicador repetir nativa/1080p/720p, y procesamiento nativo/1080p/720p. Entrada nativa de
   esta app es 1080p, por lo que esas dos selecciones deben coincidir; no declarar reducción entre ellas.
6. Guardar registros FRAME por configuración durante al menos 60 segundos. Registrar CPU por etapa,
   captura real, flips confirmados, dimensiones y presupuesto. Medir RSS mediante herramienta externa
   si existe; no sustituirlo por la suma de buffers. Para latencia mando-a-fotón hace falta medición externa.
7. Alternar 720↔1080 al menos veinte veces, desactivar y comprobar restauración 1920×1080 sin corrupción.
   Validar filas/pitch y canales de color con imágenes de patrón y capturas. Registrar rechazos de VideoOut.
8. Cerrar/reabrir: conservar preferencias, motor OFF. Probar archivo de preferencias inválido y desconexión
   del mando. Conservar el archivo anterior antes de preparar el caso inválido; restaurarlo después.
9. Si hay cuelgue, fallo de cierre, pérdida de imagen, formato incorrecto o error de flip, detener pruebas,
   conservar el diagnóstico y no cargar el motor en juegos. Un timeout o desactivación no equivale a estabilidad.

El paquete no incluye PRX ni modifica la configuración de plugins. Para detener el experimento basta salir
de la aplicación; sus preferencias en `/data/PS4FrameGen` son locales a ella. El rendimiento y la calidad
medidos en PC no sustituyen esta validación. La siguiente etapa técnica es un backend GPU medido en
esta aplicación controlada, antes de investigar cualquier título externo.
