# Arquitectura y límites

Flujo: captura controlada → reducción interna opcional → movimiento bidireccional → interpolación
en `i/M` → escalado a un buffer de salida real → presentación por un adaptador específico.

- `engine/capture`: PPM RGB de 8 bits y almacenamiento RGBA8 opaco, tamaños verificados antes de reservar.
- `engine/motion`: luminancia a 160 o 320 píxeles de ancho; desplazamiento global disperso para sembrar
  búsqueda SAD por bloques de 8×8; campos hacia delante y atrás. No usa vectores del juego.
- `engine/interpolation`: reconstrucción inversa desde ambas fuentes con dos iteraciones; mezcla
  temporal de muestras bilineales y selección del warp temporalmente más cercano cuando los campos
  son inconsistentes. Esto produce nuevas imágenes, pero puede generar deformaciones y oclusiones.
- `engine/scaling`: área de cobertura para reducción o bilineal; lienzo con barras negras para conservar
  proporción. El caso 2:1 puede producir resultados iguales entre filtros. Una petición nominal mayor
  que la entrada se rechaza; subir procesamiento reducido a una salida mayor sí es una operación explícita.
- `engine/performance`: tiempos de preparación, estimación, interpolación y escalado; contabilidad de
  buffers y alarma de presupuesto. Memoria no equivale a RSS ni VRAM. La estimación de latencia suma
  espera de un original futuro y CPU medida; excluye captura externa, mando, cola de pantalla y scanout.
- `engine/presentation`: cuenta originales adquiridos y flips confirmados en relojes monótonos, por
  separado. El CLI de archivos registra FPS de presentación y superficie como `null`.
- `overlay`: flanco de L3+R3, navegación y preferencias validadas por identificador. El motor no se
  reactiva al cargar preferencias. Textos y rectángulos se rasterizan sobre imágenes reales.
- `ps4-app`: escena analítica propia de 1920×1080 y adaptadores Pad/VideoOut. El overlay se dibuja después
  de interpolar, de modo que no se utiliza como entrada de estimación. Los registros distinguen superficie
  y modo físico de pantalla; el cambio de superficie no fuerza el modo HDMI global del sistema.

## Memoria, cambios y errores

Cada imagen admite hasta 4096×2160 y usa RAII. Las salidas de un par son como máximo cuatro buffers;
el CLI escribe fuera del tramo medido. Los tamaños incompatibles, capturas truncadas, cuadrículas de
movimiento inválidas, timestamps no monótonos y configuraciones inválidas emiten errores.
El corte de escena es un detector sencillo por diferencia media de luminancia: no cubre todos los
cortes. Cuando dispara, el motor devuelve solo el original y cero imágenes generadas.

VideoOut conserva dos slots de capacidad 1080p durante todos los cambios. Espera confirmación del
flip pendiente, cierra el handle y registra la nueva geometría con el mismo almacenamiento. No libera
un buffer durante el cambio. Antes de copiar verifica geometría y capacidad. El fallo desactiva el motor
y trata de registrar de nuevo la superficie nativa; si eso falla, termina. Un cierre fallido impide liberar
la memoria de vídeo desde el destructor; el sistema la recuperará al terminar el proceso.
Este protocolo está escrito y compilado, pero todavía necesita validación física y no se declara seguro
en todas las versiones del controlador. El timeout de flip evita un bucle de espera infinito.

Al desactivar, el procesamiento vuelve a entrada nativa y la salida al tamaño original. Las preferencias
de resolución se conservan para una activación manual posterior. La escritura es por reemplazo temporal
atómico; un fallo conserva la configuración anterior. No se modifican archivos del sistema.

## Límites de calidad y rendimiento

El muestreo de movimiento es deliberadamente reducido y la búsqueda tiene rango finito; detalles finos,
objetos rápidos independientes, HUD, disoclusiones, transparencias, cambios de iluminación y parallax pueden
fallar. El seed de traslación global ayuda con panorámicas, pero no es un estimador proyectivo completo.
Se opera sobre RGB codificado de 8 bits; no hay gestión HDR, alpha de composición, profundidad ni espacio
lineal de color. No se admite superficie tiled ni formato arbitrario como entrada del motor.
No hay backend compute GCN, optical flow neuronal, inserción en juegos, ni promesa de mejora universal.
