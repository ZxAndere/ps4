# Rendimiento y calidad 0.2

Se continuó el proyecto 0.1 sin eliminar sus artefactos. Fuente del benchmark: cinco fotogramas consecutivos de vídeo 1920×1080 a 25 Hz; las imágenes intermedias de referencia no se usan para estimar movimiento. Todos los tiempos son de CPU por par, no de un único fotograma ni FPS de pantalla.

## Comparación intercalada a procesamiento/salida 720p ×2

Quince rondas por variante, un calentamiento, orden pseudoaleatorio fijo. No se decodifican/escriben imágenes entre pares medidos. Host Debian 13, GCC 14.2, AMD EPYC 9V74, cuota de cuatro CPU y sin dispositivo GPU. Compilación genérica x86-64/SSE2; no se usa AVX2, AVX-512 ni fast-math. Esta es la comparación principal porque reduce el sesgo del orden de ejecución.

| Variante | Mediana ms | P10–P90 ms | Aceleración | Reservas de imágenes por par tras calentar | Copia de imágenes MiB/par |
|---|---:|---:|---:|---:|---:|
| reference | 230.94 | 223.27–241.03 | 1.00× | 6 | 10.55 |
| scalar | 124.60 | 117.83–132.44 | 1.85× | 8 | 7.03 |
| sse2 | 106.46 | 104.31–110.95 | 2.17× | 8 | 7.03 |
| direct | 107.61 | 101.88–111.81 | 2.15× | 7 | 3.52 |
| reuse | 107.22 | 103.77–112.89 | 2.15× | 0 | 3.52 |
| threads2 | 64.66 | 55.68–73.80 | 3.57× | 0 | 3.52 |
| threads4 | 42.32 | 30.43–52.37 | 5.46× | 0 | 3.52 |
| motion-early | 41.48 | 33.65–43.58 | 5.57× | 0 | 3.52 |

El resultado de referencia es aproximadamente 231 ms frente a 41.5 ms del conjunto optimizado: unas 5.6 veces más rápido en este host. Todas las variantes de esta tabla dan **MSE 0** contra la referencia para la imagen intermedia evaluada. La matriz separada también compara los intermedios ×3/×4 a 720p contra el ejecutable 0.1 preservado: diferencia máxima de canal cero.

`--simd off` desactiva los intrínsecos explícitos; el compilador puede seguir autovectorizando el C++ escalar. Las cifras no comparan arquitecturas distintas ni un modo con fast-math.

SSE2 y los kernels escalares aportan una mejora medible. Quitar copias y reutilizar buffers reduce tráfico/reservas, pero su cambio de tiempo aislado queda dentro del ruido de esta prueba; no se presenta cada fila como una mejora garantizada. La poda SAD reduce modestamente la etapa de movimiento: de 3.165 a 2.774 ms de mediana al comparar cuatro hilos con/sin poda. El total incluye otros costes y ruido del host compartido.

Las reservas contadas son exclusivamente las de buffers `Image`, no todos los malloc, pilas de hilos ni RSS. Aún se crean pequeños buffers de luminancia/campos y metadatos. Las copias son las del motor durante el par; la captura cooperativa y la copia a memoria VideoOut añaden tráfico fuera de esa medición. El modo sin reutilización reconstruye su scratch explícitamente y puede reservar más imágenes que el backend 0.1; es un control experimental, no el camino recomendado.

## Matriz de resoluciones y multiplicadores

Nueve repeticiones por configuración después de calentamiento, en invocaciones CLI separadas. Se conservan mínimo, mediana, máximo, etapas y PSNR en `measurements-v02.json`. Por variación de frecuencia/planificación, estas medianas pueden diferir de la serie intercalada; no se selecciona su mejor mínimo como resultado principal.

| Variante | Multiplicador | Proceso | Salida | Mediana ms | Mín–máx ms |
|---|---:|---|---|---:|---:|
| retained-v01 | ×2 | 1280×720 | 1280×720 | 245.95 | 236.75–281.68 |
| motion-early | ×2 | 1280×720 | 1280×720 | 31.68 | 31.31–43.05 |
| retained-v01-x3 | ×3 | 1280×720 | 1280×720 | 381.70 | 375.98–395.93 |
| retained-v01-x4 | ×4 | 1280×720 | 1280×720 | 565.56 | 544.73–614.76 |
| process-1080-x2 | ×2 | 1920×1080 | 1280×720 | 70.41 | 59.11–87.35 |
| process-540-x2 | ×2 | 960×540 | 1280×720 | 36.11 | 34.24–36.55 |
| process-360-x2 | ×2 | 640×360 | 1280×720 | 25.23 | 24.18–25.66 |
| process-1080-x3 | ×3 | 1920×1080 | 1280×720 | 139.17 | 111.74–158.03 |
| process-540-x3 | ×3 | 960×540 | 1280×720 | 59.90 | 46.70–76.69 |
| process-360-x3 | ×3 | 640×360 | 1280×720 | 37.94 | 33.92–40.46 |
| process-720-x3 | ×3 | 1280×720 | 1280×720 | 72.17 | 63.95–75.73 |
| process-1080-x4 | ×4 | 1920×1080 | 1280×720 | 207.28 | 194.36–227.68 |
| process-540-x4 | ×4 | 960×540 | 1280×720 | 74.59 | 60.69–81.01 |
| process-360-x4 | ×4 | 640×360 | 1280×720 | 49.45 | 36.20–61.72 |
| process-720-x4 | ×4 | 1280×720 | 1280×720 | 92.45 | 79.96–109.49 |

## Calidad y presupuesto

Las optimizaciones a igual resolución conservan PSNR **28.371615 dB** en ×2/720p, idéntico a 0.1. El original temporalmente más cercano da 26.920944 dB. Se mantienen las pruebas de traducción conocida, tiempos ×2/×3/×4, cortes, dimensiones, desactivación y ahora también determinismo paralelo, filtros/letterbox y reutilización.

| Procesamiento, salida 720p ×2 | PSNR generado dB | PSNR original cercano dB |
|---|---:|---:|
| 720p | 28.371615 | 26.920944 |
| 540p | 28.658995 | 26.920944 |
| 360p | 28.974039 | 26.920944 |

Un PSNR mayor al procesar a menor resolución puede venir de suavizar aliasing y errores del estimador; **no demuestra mayor detalle**. Procesar a 360p descarta información que el escalado posterior no recupera. Se guardan las imágenes y referencias para inspección; no se promete esa mejora en otros contenidos.

720p ×2 sigue superando los 33.3 ms por par en la mediana intercalada y presenta variación importante. Procesar a 360p/output 720p dio unos 25.2 ms por par ×2 en la matriz separada, pero ni esa cifra ni el presupuesto de CPU demuestran 60 flips/s. Los modos ×3/×4 cuestan más; una pantalla de 60 Hz no presenta 90/120 imágenes únicas/s.

El intervalo de los extremos del vídeo es M×40 ms, porque se retienen los fotogramas intermedios como referencia. El campo `over_budget` del CLI se refiere a esa cadencia declarada; no debe confundirse con el objetivo externo de una fuente de 30 Hz. Las estimaciones de espera+CPU no son latencia mando-a-fotón. No se han medido FPS físicos, GPU, consumo de memoria real ni latencia en una PS4.

## Incidencias y reproducibilidad

La primera aproximación sustituyó la aritmética de coordenadas y el umbral de oclusión por formas que cambiaban algunos píxeles hasta 21 niveles. El comparador la rechazó. Se restauró la aritmética y se conserva la comparación hypot de referencia cerca del umbral; las salidas finales son idénticas en los pares de igual resolución comprobados. Los intentos rechazados permanecen en artifacts, no se usan como evidencia de calidad.

La escena propia ahora calcula trigonometría por eje. Los dos rasters analíticos 2560×1440 coinciden exactamente por SHA-256 con 0.1; este cambio acelera la fuente de prueba sin convertirlo en una cifra de rendimiento del motor.

Para repetir: compilar con `scripts/build-host.sh`, ejecutar `scripts/compare-optimizations.py` con una ruta nueva y usar `framegen_ablation` como muestra el README. El segundo comparador registra todas las muestras crudas y su semilla. Los JSON, logs y artefactos previos se conservan. Ningún resultado acredita compatibilidad con firmware 13.52, GoldHEN beta 12 ni juegos externos.
