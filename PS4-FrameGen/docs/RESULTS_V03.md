# Resultados 0.3: CPU, calidad y límites

## Medición principal intercalada

Linux x86-64, AMD EPYC 9V74, GCC 14.2.0 Release, cuatro hilos. Cgroup: cuatro CPU de cuota; cinco
CPU en afinidad. PS4 Fat/Jaguar y Windows no se ejecutaron. El destino PS4 se compila con Clang/LLD
19.1.7 y dos hilos, SSE2. No se extrapolan estos milisegundos a PS4.

31 rondas, 28 variantes en orden pseudoaleatorio reproducible (semilla 1179797299), calentamiento
por variante, sin I/O entre pares cronometrados y sin compilaciones/tests/replay concurrentes.
Entradas reales 0/2 del vídeo en todas las variantes intercaladas (par de 80 ms), salida 1280×720;
el temporizador incluye preparación, movimiento, warp
y escalado CPU. Excluye decodificación, overlay, copia de captura PRX, VideoOut y scanout.
Los percentiles reflejan la variabilidad del host compartido. Las medianas de etapas no se suman
necesariamente a la mediana del total.

| Proceso ×2 → salida 720p | Ruta equivalente 0.2 | 0.3 por defecto | p10–p90 de 0.3 | Reducción |
|---|---:|---:|---:|---:|
| 720p | 42.64 ms | 41.58 ms | 31.79–45.13 ms | 2.5 % |
| 540p | 36.74 ms | 28.12 ms | 20.82–30.94 ms | 23.5 % |
| 360p | 26.77 ms | 17.30 ms | 13.09–18.20 ms | 35.4 % |

La diferencia a 720p es pequeña frente a la dispersión: no se acredita una mejora importante ahí.
41,58 ms todavía exceden un presupuesto CPU de 33,33 ms/par. El ahorro principal aparece al
reducir a 540/360p: sumas enteras SIMD en footprints Area y coordenadas bilineales en caché.
A 360p la preparación pasa de 4,84 a 1,64 ms y el escalado final de 11,39 a 5,27 ms; el warp
permanece alrededor de 7,6–7,7 ms. Se mantienen cero asignaciones de imágenes tras calentar en
estos modos sin nitidez; luminancia/flujo y contenedores aún pueden asignar. No se afirma malloc cero.
La memoria reportada suma capacidad RGBA retenida, incluida tras reducir dimensiones; no es RSS,
heap total, stacks ni memoria GPU.

La ruta «equivalente 0.2» desactiva vector_flow, integer_scale y cached_scale. Un comparador separado
ejecuta además el binario 0.2 congelado: 43,49→44,64 ms a 720p, 35,06→27,66 ms a 540p y
24,83→17,03 ms a 360p (nueve repeticiones por proceso). No se mezclan esas medianas con la
ablación intercalada para atribuir ganancias. Las cifras históricas 230,9/41,5/25,2 ms permanecen
en sus informes 0.1/0.2.

## Ablaciones y compromisos

| Variante | Mediana CPU | Interpretación |
|---|---:|---|
| SIMD de componentes de flujo, 720p | 41.71 ms | Véase flags y muestras crudas |
| Flujo SIMD + reducción entera, 540p | 32.40 ms | Véase flags y muestras crudas |
| Flujo SIMD + reducción entera, 360p | 22.57 ms | Véase flags y muestras crudas |
| Anterior + caché de coordenadas, 540p | 27.52 ms | Véase flags y muestras crudas |
| Anterior + caché de coordenadas, 360p | 19.92 ms | Véase flags y muestras crudas |
| Una iteración, 720p | 37.01 ms | Calidad modificada; experimental |
| Una iteración + caché, 360p | 15.91 ms | Calidad modificada; experimental |

El SIMD adicional del flujo tiene resultados variables; queda desactivado por defecto. El SSE2
de muestreo/mezcla ya existente continúa activo. El objeto Orbis contiene mulps/addps/divps, paddw,
punpcklbw y cvttps2dq, ver `validation-v03/sse2.json`; esto demuestra compilación, no ejecución Jaguar.
Una iteración reduce trabajo, pero no conserva píxeles: respecto a dos iteraciones, error máximo
RGB de 82 a 720p y 56 a 360p en este par; MAE 0,437 y 0,202. Auto puede usarla al degradar el
nivel. No se presenta como sustitución equivalente ni garantía contra artefactos.

## Calidad y escaladores

Vídeo OpenCV `1920x1080.avi`, 25 Hz verificados; entradas 0 y M, referencias retenidas 1…M−1.
Commit del dataset: `42a04e79550c8a58a0ec1b41aa2bce25236dbb0e`; SHA-256 del vídeo
`42f77d89c19428a06c70e3c8f455cc3cced40bd0b46dafee3bfbfde323362153`.
Para ×2 el intervalo real es 80 ms; para ×3, 120 ms; ×4, 160 ms. No es un vídeo de entrada a 30 Hz.
Las PPM canónicas se incluyen en el ZIP para repetir sin inventar timestamps. La referencia de
calidad siempre se reduce con Area a 720p, incluso cuando cambia el filtro del candidato.

Comparaciones ×2 a 720/540/360 y todos los intermedios ×3/×4 a 720 coinciden exactamente entre
0.2 y 0.3 en los modos equivalentes. El informe contiene 17 configuraciones, más la referencia.
Las pruebas analíticas de traducción, bordes y tamaños impares siguen activas.

| Variante ×2, salida 720p | CPU por proceso, mediana | PSNR fijo | Error medio de bordes horizontales |
|---|---:|---:|---:|
| v03-720 | 44.64 ms | 28.3716 dB | 2.4212 |
| v03-540 | 27.66 ms | 28.6590 dB | 2.3744 |
| v03-360 | 17.03 ms | 28.9740 dB | 2.4172 |
| one-iteration-720 | 37.03 ms | 28.4872 dB | 2.4228 |
| one-iteration-360 | 14.67 ms | 29.0547 dB | 2.4209 |
| nearest-360 | 15.55 ms | 28.1401 dB | 3.2389 |
| bilinear-360 | 17.15 ms | 28.8588 dB | 2.4185 |
| bicubic-360 | 72.23 ms | 28.7747 dB | 2.4580 |
| sharp25-360 | 29.33 ms | 28.9913 dB | 2.4121 |
| v03-1080 | 76.83 ms | 27.9141 dB | 2.5034 |

La reducción 360p pierde detalle aunque su PSNR sea mayor en este clip por el suavizado.
Vecino cercano cuesta menos y aumenta el error de bordes (3,24 frente a ~2,42); bicúbico cuesta
~72 ms por par y no mejora aquí el PSNR. Nitidez 25 cuesta ~29 ms frente a ~17 ms sin ella.
Bicúbico/nitidez son opciones de comparación, no recomendaciones de rendimiento. El algoritmo no
reconstruye detalle que no existe en la resolución reducida. Un clip y los tests analíticos no
demuestran superioridad general; se conserva también la evidencia negativa de versiones anteriores.

## Controlador, deadlines y baja latencia

El replay final repite el mismo par ya decodificado durante 96 iteraciones por ruta con presupuesto
de 33,33 ms. Usa costes CPU y reloj reales; no representa una captura de juego ni una pantalla.
El promedio se reinicia al cambiar de nivel para que el coste anterior no fuerce degradaciones
consecutivas indebidas; una regresión específica comprueba ese comportamiento.

| Replay final | Normal | Baja latencia |
|---|---:|---:|
| Coste CPU mediano | 24.20 | 23.67 |
| Antigüedad estimada del original al estar listo | 57.53 | 56.30 |
| Pares con bypass al original actual | 0 | 5 |
| Generados descartados | 0 | 10 |
| Nivel final de presión | 4 | 5 |

Los valores temporales de esa tabla están en ms. La diferencia de medianas **no prueba menor
latencia física**: son ejecuciones secuenciales y terminaron con niveles distintos. Verifica el
bypass y los descartes bajo coste real; los deadlines y el limitador se prueban además con
casos deterministas. Ambas rutas tienen cero flips confirmados y presented_fps/input-to-photon
null. No se convierte throughput CPU en FPS presentados. El limitador 30/60 solo se conecta a
presentación propia con ruta VideoOut 60 Hz; rechaza objetivos superiores a la frecuencia declarada
en el planificador host. Su funcionamiento con la pantalla física aún requiere prueba.

## Evidencia y reproducibilidad

- `artifacts/v03/interleaved-verified.json`: muestras crudas y flags de ablación implícitos en el ejecutable.
- `artifacts/v03/quality/summary.json`: métricas, configuraciones y comparación contra binario anterior.
- `artifacts/v03/comparison-final.html`: originales, referencia, 17 candidatos y diferencia ×4; controles
  JavaScript probados con fixture DOM en Node. El renderizado en navegador no se automatizó.
- `artifacts/v03/replay-final`: JSONL de costes, decisiones, estimaciones y descartes.
- `docs/validation-v03`: 37 casos funcionales, integración PRX, ASan/UBSan, plataforma, binarios y PKG.
- `artifacts/v03/recovery-v02/sha256.json`: recuperación y conservación de entregas previas.

`interleaved-final.json` fue una iteración de desarrollo que coincidió con replay y se excluye de
las conclusiones; también se conservan los prototipos y el primer replay cuyo EWMA motivó la
corrección. La medición utilizada aquí es únicamente `interleaved-verified.json`. Los cambios
posteriores a ese benchmark fueron política adaptativa, contabilidad de memoria, recuperación y
overlay, fuera del kernel cronometrado. Se recompilaron y probaron antes de archivar la entrega.

No hay medición GPU, consola, Windows ni latencia mando-a-fotón. Antes de juegos reales faltan
prueba física de loader/firmware y app, adquisición/fences/layout de superficies por juego,
presentación sincronizada y recursos de procesamiento suficientes. Ver GPU_GOLDHEN_V03.md.
