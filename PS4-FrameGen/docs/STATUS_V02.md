# Estado de la continuación 0.2

## Implementado y ejecutado

- Kernels escalares con acceso directo a buffers validados, escalado con planes reutilizables y descarte
  de cálculos de campo no usados. La comparación de oclusiones conserva el redondeo de referencia cerca
  del umbral; una primera aproximación se rechazó por diferencias de píxeles y quedó preservada para diagnóstico.
- SIMD SSE2 para muestreo RGBA y mezcla. Se verificaron las instrucciones en el objeto de destino PS4.
- Pool acotado de 1…8 hilos persistentes por Processor, partición por filas y joins al cerrar.
- Eliminación de copias de preparación a resolución nativa y de la copia intermedia cuando proceso y
  salida coinciden. Reutilización de buffers de imágenes, planes y resultados entre pares.
- Poda conservadora del SAD por filas que evita candidatos incapaces de mejorar el ganador. Sus campos,
  costes y salidas se comparan con la búsqueda completa; se conserva la opción de usarla como referencia.
- Procesamiento real de 960×540 y 640×360, escalado a salida 720p/1080p/nativa y selección conectada al menú.
- Generación separable de la escena propia: senos/cosenos calculados por eje. Ambos originales de 2560×1440
  coinciden byte a byte con los rasters 0.1 conservados.
- PRX cooperativo con ABI C, captura de superficies propias, controles y presentación por callback. Prueba
  host con la implementación compartida real. Integración de carga opcional en el código de la aplicación PS4.

## Validación

- 22 casos funcionales pasan; cero fallos y cero omitidos.
- Una segunda suite de integración prueba la ABI cooperativa, L3+R3, captura, interpolación, superficies
  rechazadas, reentrada y fallos de callback. CTest completa ambas suites.
- Ambas suites pasan con ASan y UBSan, sin diagnósticos. No es una prueba del kernel o controlador de PS4.
- 22 configuraciones en la matriz de rendimiento/PSNR, nueve repeticiones después de calentamiento.
- Ocho variantes intercaladas durante quince rondas, orden pseudoaleatorio fijo y sin I/O de imágenes entre
  pares medidos. Todas conservan MSE cero frente al backend de referencia en la secuencia de igual resolución.
- PKG y PRX compilados; comprobación de dependencias, exportaciones, SFO, hashes/firmas y extracción con
  igualdad de payloads. Los hashes de los artefactos listados en el manifiesto 0.1 se mantienen intactos.

Los detalles están en `OPTIMIZATION_V02.md`, `measurements-v02.json`, `interleaved-v02.json`,
`PKG_V02.md`, `GOLDHEN_V02.md` y `validation-v02/`.

## Pendiente

Instalar/ejecutar en una PS4 accesible y comprobar el firmware/GoldHEN reales, creación de hilos, superficies,
presentación y ciclo de carga/descarga del PRX. No hay acceso a esa consola desde este entorno.
El modo de procesamiento reducido puede mejorar throughput en el host; no prueba 30→60 FPS en PS4.

Dentro de juegos reales faltan un adaptador específico de captura con ownership/fences/formato demostrado,
integración con su cola de presentación y un presupuesto de CPU/GPU medido. La integración no se considera
universal ni terminada. La siguiente prueba debe realizarse en la aplicación propia antes de tocar juegos.

## Conservación

`artifacts/optimization-v02/baseline/` conserva ejecutable, código fuente y PKG anteriores. `build/`,
`build-ps4/`, los resultados 0.1 y su ZIP no se eliminaron. Los intentos de comparación rechazados y los
resultados preliminares también siguen en `artifacts/optimization-v02/`; solo se cita como final la matriz
`final-comparison` y la serie intercalada. Los nuevos informes tienen nombres 0.2 separados de los anteriores.
