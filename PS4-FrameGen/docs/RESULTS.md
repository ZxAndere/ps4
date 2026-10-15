# Resultados CPU reproducibles

No son FPS de pantalla ni resultados de PS4. Tiempo por par de originales; incluye todos los intermedios del par. Mediana de las repeticiones, tras calentamiento. Entrada analítica: 2560×1440; OpenCV: dimensiones decodificadas.

| Caso | Entrada | Proceso | Buffer de salida | Total mediana ms | Escalado medio ms | Memoria imágenes MiB |
|---|---|---|---|---:|---:|---:|
| analytic_x2_native_native | 2560×1440 | 2560×1440 | 2560×1440 | 649.64 | 7.27 | 84.4 |
| analytic_x2_native_1080 | 2560×1440 | 2560×1440 | 1920×1080 | 811.43 | 175.54 | 72.1 |
| analytic_x2_native_720 | 2560×1440 | 2560×1440 | 1280×720 | 694.05 | 78.59 | 63.3 |
| analytic_x2_1080_1080 | 2560×1440 | 1920×1080 | 1920×1080 | 530.77 | 4.25 | 59.8 |
| analytic_x2_720_720 | 2560×1440 | 1280×720 | 1280×720 | 236.28 | 1.71 | 42.2 |
| analytic_x3_native_native | 2560×1440 | 2560×1440 | 2560×1440 | 1232.39 | 11.08 | 98.4 |
| analytic_x3_native_1080 | 2560×1440 | 2560×1440 | 1920×1080 | 1508.31 | 271.28 | 80.0 |
| analytic_x3_native_720 | 2560×1440 | 2560×1440 | 1280×720 | 1413.56 | 122.02 | 66.8 |
| analytic_x3_1080_1080 | 2560×1440 | 1920×1080 | 1920×1080 | 871.16 | 5.95 | 67.7 |
| analytic_x3_720_720 | 2560×1440 | 1280×720 | 1280×720 | 378.40 | 2.64 | 45.7 |
| analytic_x4_native_native | 2560×1440 | 2560×1440 | 2560×1440 | 1823.21 | 14.33 | 112.5 |
| analytic_x4_native_1080 | 2560×1440 | 2560×1440 | 1920×1080 | 2322.06 | 378.52 | 87.9 |
| analytic_x4_native_720 | 2560×1440 | 2560×1440 | 1280×720 | 1937.03 | 155.27 | 70.3 |
| analytic_x4_1080_1080 | 2560×1440 | 1920×1080 | 1920×1080 | 1241.92 | 8.48 | 75.6 |
| analytic_x4_720_720 | 2560×1440 | 1280×720 | 1280×720 | 568.20 | 4.07 | 49.2 |
| opencv_x2_native_native | 1920×1080 | 1920×1080 | 1920×1080 | 371.02 | 3.97 | 47.5 |
| opencv_x2_native_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 359.97 | 3.96 | 47.5 |
| opencv_x2_native_720 | 1920×1080 | 1920×1080 | 1280×720 | 450.24 | 80.09 | 38.7 |
| opencv_x2_1080_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 361.56 | 3.75 | 47.5 |
| opencv_x2_720_720 | 1920×1080 | 1280×720 | 1280×720 | 234.19 | 1.70 | 29.9 |
| opencv_x3_native_native | 1920×1080 | 1920×1080 | 1920×1080 | 707.05 | 5.93 | 55.4 |
| opencv_x3_native_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 701.98 | 5.80 | 55.4 |
| opencv_x3_native_720 | 1920×1080 | 1920×1080 | 1280×720 | 813.55 | 114.74 | 42.2 |
| opencv_x3_1080_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 727.50 | 5.84 | 55.4 |
| opencv_x3_720_720 | 1920×1080 | 1280×720 | 1280×720 | 396.90 | 2.84 | 33.4 |
| opencv_x4_native_native | 1920×1080 | 1920×1080 | 1920×1080 | 1070.68 | 7.84 | 63.3 |
| opencv_x4_native_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 1097.81 | 8.29 | 63.3 |
| opencv_x4_native_720 | 1920×1080 | 1920×1080 | 1280×720 | 1207.04 | 156.31 | 45.7 |
| opencv_x4_1080_1080 | 1920×1080 | 1920×1080 | 1920×1080 | 1099.82 | 8.02 | 63.3 |
| opencv_x4_720_720 | 1920×1080 | 1280×720 | 1280×720 | 549.21 | 3.54 | 36.9 |
| filter_1080_area | 2560×1440 | 2560×1440 | 1920×1080 | 792.42 | 173.64 | 72.1 |
| filter_1080_bilinear | 2560×1440 | 2560×1440 | 1920×1080 | 744.31 | 105.37 | 72.1 |
| filter_720_area | 2560×1440 | 2560×1440 | 1280×720 | 716.17 | 81.07 | 63.3 |
| filter_720_bilinear | 2560×1440 | 2560×1440 | 1280×720 | 666.84 | 45.16 | 63.3 |

La calidad por fotograma está en `summary.json`: PSNR frente a la imagen retenida y frente al original temporalmente más cercano. PSNR no mide todos los defectos perceptivos. Los recortes sintéticos tienen movimiento conocido; los fotogramas OpenCV no se usan para ajustar el estimador. Memoria de imágenes es una contabilidad de buffers, no RSS ni VRAM.

## Interpretación y calidad

Host Debian 13 x86-64, GCC 14.2, perfil performance y un solo hilo de cómputo del motor.
Máquina compartida sin afinidad ni aislamiento de CPU; los tiempos de pared son orientativos.
Las 34 configuraciones completaron tres repeticiones medidas después de calentamiento.
Se verificaron las cabeceras y longitud real de cada raster PPM contra las dimensiones registradas.
Las dimensiones del buffer de salida están comprobadas; no existe una superficie de pantalla en este runner.

Vídeo original: 1920×1080, 25 Hz, timestamps [0, 0.04, 0.08, 0.12, 0.16] s.
Para ×M los extremos son 0 y M, por lo que su intervalo es M×40 ms, no 40 ms.
Las referencias intermedias se reservan y no participan en estimar movimiento.
El escenario analítico nativo es 2560×1440 y permite comprobar reducción real tanto a 1080p como a 720p.
En el vídeo, nativa y 1080p coinciden por definición y sus diferencias de tiempo son ruido de medición.

| Vídeo ×2 | PSNR generado dB | PSNR original cercano dB |
|---|---:|---:|
| Proceso/salida 1080p | 27.567 | 26.581 |
| Proceso 1080p → salida 720p | 27.914 | 26.921 |
| Proceso/salida 720p | 28.372 | 26.921 |

Todos los intermedios del vídeo consecutivo evaluado mejoran el PSNR frente al original cercano,
pero esta secuencia no demuestra calidad general. La reducción elimina detalle, suaviza aliasing y
puede subir el PSNR al eliminar diferencias de alta frecuencia; una cifra mayor a 720p no implica
una imagen más detallada que a 1080p. Comparar las imágenes y la métrica dentro de cada resolución.
El motor puede deformar contornos, crear arrastres y fallar en disoclusiones o cambios de iluminación.

Se conserva un ensayo exploratorio adicional en `exploratory-quality.json`: los PNG de optflow
`1080p_00/01/02` dieron 18.068 dB frente a 18.252 dB del original cercano con proceso/salida 720p.
Es un caso peor para el estimador. Su separación temporal no estaba verificada y no se usa como
referencia temporal principal; no se oculta ni se utiliza para afirmar una mejora universal.

Área y bilineal se compararon a 1080p y 720p con el mismo patrón analítico. A razón exacta 2:1
pueden coincidir al promediar los mismos cuatro píxeles; la relación 4:3 de reducción a 1080p
produce pequeñas diferencias. Es un patrón suave y no representa todos los contenidos.

Los tiempos son por par completo, con 1/2/3 intermedios. No son tiempo de un único fotograma.
Reducir solo la salida añade escalado al trabajo de interpolación nativa; reducir el procesamiento
también reduce ese trabajo. Ninguno de los resultados demuestra capacidad de 30→60 FPS.
La estimación de espera + CPU no es una medida mando-a-fotón y no incluye cola de vídeo ni scanout.
No se extrapola al Jaguar/GCN de PS4.

## Imágenes para inspección

- `figures/original-a-1080.png`: original A del vídeo.
- `figures/generated-x2-1080.png` y `figures/truth-x2-1080.png`: generado y referencia retenida, 1920×1080.
- `figures/generated-x2-720.png` y `figures/truth-x2-720.png`: generado y referencia reducida, 1280×720.
- `figures/overlay-host-720.png`: raster del menú en host, sin presentación física.

Las imágenes PNG se exportaron de los PPM sin redimensionar. Los PPM y todos los intermedios
permanecen en `artifacts/final/`. No se aplicaron correcciones manuales a los resultados.
