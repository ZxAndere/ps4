# Registro de la primera entrega

El objetivo universal sigue abierto. Se entrega una implementación real de referencia, ejecutada
en el host, y una aplicación nativa compilada que necesita validación en PS4.

| Fase | Implementado / comprobado | Pendiente / siguiente paso |
|---|---|---|
| 0 Auditoría | Herramientas host, descarga oficial de SDK, API de vídeo/pad, SDK GoldHEN y límites documentados | Identificar GoldHEN completo y verificar firmware 13.52; auditar Windows real |
| 1 Motor | C++ modular; movimiento bidireccional; ×2/×3/×4; filtros; procesamiento/salida separados; exportación y referencias | Optimizar calidad en oclusiones; backend GPU y presupuesto de tiempo real |
| 2 App PS4 | Escena propia, adaptadores de mando/vídeo, buffers de 1080p/720p, configuración y registros; ELF/PKG compilados | Instalar y ejecutar en consola; confirmar resolución/formato de superficies y flips |
| 3 Overlay / PRX | Menú rasterizado y controles L3+R3 con pruebas host; investigación documentada | Verificar mando y overlay nativos; no se implementó PRX ni hooks de juegos |
| 4 Pruebas | 17 casos funcionales host, sanitizadores ASan/UBSan, benchmark de imágenes sintéticas y vídeo consecutivo | Estabilidad, memoria, latencia y presentación física en PS4; pruebas dentro de juegos |
| 5 Paquete | Fake PKG creado, hashes/firmas estructurales validados | Instalación y ejecución en consola compatibles; no se declara probado en PS4 |

## Comandos ejecutados

- `bash scripts/setup-cloud.sh`: instalación y compilación host/PS4. También ejecutado con
  `FRAMEGEN_TOOLS_ROOT=/workspace/framegen-repro` para descargar y preparar herramientas desde cero.
- `bash scripts/build-host.sh` y `build/framegen_tests`: **17 pasan, 0 fallan, 0 omitidos**.
  CTest registra una suite que ejecuta esos 17 casos; no son 17 entradas independientes de CTest.
- Configuración Debug con `-DFRAMEGEN_SANITIZE=ON`, compilación y CTest: misma suite completada con
  AddressSanitizer y UndefinedBehaviorSanitizer, sin diagnósticos de memoria o comportamiento indefinido.
- `bash scripts/fetch-test-video.sh`: descarga fijada y SHA-256 correcto.
- `python3 scripts/benchmark.py --video data/opencv-1920x1080.avi --output artifacts/final --repeats 3`:
  matriz de resoluciones/multiplicadores, imágenes exportadas, tiempos medidos y calidad contra referencias.
- `bash scripts/build-ps4.sh`: ELF/OELF/eboot y PKG. `pkg_validate`: comprobaciones `[OK]`, sin fallos.
  La tabla dinámica del ELF importa libkernel, SceVideoOut, ScePad y SceUserService; no bibliotecas Linux.

## Archivos y persistencia

El proyecto contiene motor, adaptadores nativos, menú, CLI, tests, CMake, scripts y documentación.
`build/` guarda ejecutables host; `build-sanitize/`, instrumentos de prueba; `build-ps4/`, el paquete.
`artifacts/final/` conserva fotogramas, referencias, menú rasterizado y mediciones JSON por caso.
`docs/RESULTS.md` y `docs/measurements.json` guardan el informe de la ejecución final.

Las herramientas instaladas permanecen fuera del checkout en `/workspace/framegen-toolchains`.
`scripts/cloud-env.sh` activa las rutas para una nueva shell; no se presupone que procesos sobrevivan.
No hay servidores que arrancar. No se ha publicado el entorno ni verificado restauración en otra tarea.
El repositorio empezó sin commits; los archivos creados están en el workspace, sin push ni commit automático.

La receta reproduce la construcción y validación; el PKG puede incorporar metadatos temporales y
no se exige identidad binaria entre paquetes reconstruidos. Los resultados de calidad y tiempos deben
volver a medirse si cambia el motor. El informe distingue buffers, capturas y superficies presentadas.
