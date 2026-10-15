# PS4 FrameGen 0.3 — continuación verificada en host

CPU con escalado en caché y reducción SIMD exacta, Runtime adaptativo, descarte por plazos,
modo seguro, perfiles recuperables, overlay por secciones, JSONL y comparador visual.
Se conservan las entregas 0.1/0.2 y sus pruebas.

- [Estado y pruebas 0.3](docs/V03.md)
- [Resultados y calidad](docs/RESULTS_V03.md)
- [Instalación, Windows y recuperación](docs/INSTALL_V03.md)
- [GPU y GoldHEN: evidencia y pendientes](docs/GPU_GOLDHEN_V03.md)
- Entregables reales: `artifacts/releases/0.3` (ZIP, PKG, PRX, EXE/DLL Windows y comparador HTML).

**Sin ejecutar en PS4 ni Windows. No se acredita firmware 13.52, GoldHEN «beta 12», GPU o juegos externos.**
Los tiempos publicados son CPU Linux EPYC, con cuatro hilos; el binario PS4 usa dos hilos.

```bash
source scripts/cloud-env.sh
bash scripts/build-host.sh
```

El build actual es `build-v03`; `build-opt` y las versiones anteriores se conservan como referencias.
Las secciones siguientes mantienen el historial de las entregas previas.

---

# PS4 FrameGen — actualización 0.2

Continuación de la implementación anterior, con kernels CPU/SSE2, pool de hilos, reutilización de
buffers y procesamiento adicional a 540p/360p. Los artefactos 0.1 se conservan. El nuevo PRX es
cooperativo y solo recibe buffers suministrados explícitamente por la aplicación propia.

Ver [rendimiento y calidad 0.2](docs/OPTIMIZATION_V02.md), [PRX y controles conectados](docs/GOLDHEN_V02.md),
[comprobaciones del PKG](docs/PKG_V02.md), [investigación GPU](docs/GPU_V02.md) y [estado 0.2](docs/STATUS_V02.md).

```bash
cd /workspace/ps4/PS4-FrameGen
source scripts/cloud-env.sh
bash scripts/build-host.sh
build-opt/framegen_cli --demo --process 360 --output 720 --multiplier 2 --output-dir artifacts/demo-v02-new
bash scripts/build-ps4.sh
```

El build host actual está en `build-opt`; el destino nativo nuevo, en `build-ps4-v02-final`.
Se admite `--threads 1..8`, `--simd on|off`, `--kernels fast|reference`, `--reuse on|off`,
`--direct on|off` y `--motion full|early` para comparaciones. El motor nativo usa dos hilos por defecto;
las mediciones de cuatro hilos son del host EPYC y no equivalen al rendimiento de un Jaguar.

Para repetir comparaciones, elegir rutas nuevas: el comparador y el verificador se niegan a
reemplazar informes previos. Ejecutar benchmarks sin compilaciones ni sanitizadores simultáneos.

```bash
python3 scripts/compare-optimizations.py --output artifacts/optimization-v02/comparison-new
build-opt/framegen_ablation artifacts/final/decoded/0.ppm artifacts/final/decoded/2.ppm artifacts/optimization-v02/interleaved-new.json 15
python3 scripts/verify-ps4-artifacts.py --output artifacts/optimization-v02/pkg-check-new
```

El PKG no se ha ejecutado en PS4. No se acredita firmware 13.52 ni GoldHEN «beta 12». Sigue pendiente
demostrar generación y presentación en juegos; no hay hooks de juegos ni backend GPU activados.

---

# Entrega 0.1: documentación histórica conservada

Motor C++ que estima movimiento entre dos imágenes y construye imágenes intermedias distintas.
Incluye escalado real, pruebas portátiles y una aplicación nativa de PS4 empaquetable con OpenOrbis.
No es un generador de fotogramas universal para juegos. El backend actual es CPU y no cumple
presupuestos de tiempo real a las resoluciones medidas.

## Estado comprobado

- Compilación y ejecución de la aplicación de archivos en Linux x86-64.
- Modos ×2/×3/×4 con 1/2/3 imágenes intermedias y tiempos `i/multiplicador`.
- Buffers de salida reales de 1920×1080 y 1280×720, además de resolución nativa.
- Perfiles, resolución interna independiente, dos filtros y letterboxing.
- Modelo del menú L3+R3, persistencia por identificador y activación manual en cada proceso.
- Compilación cruzada de la aplicación nativa y generación/validación estructural del `.pkg`.

**Sin comprobar en hardware:** ejecución e instalación del paquete, mando PS4, cambios de superficie,
estabilidad, FPS, latencia y consumo de memoria en consola. No se ha probado Windows físicamente.
Datos aportados por el usuario: PS4 Fat, firmware **13.52**, GoldHEN **«beta 12»**; compatibilidad pendiente.
No se entrega un PRX para juegos ni se modifican firmware o archivos del sistema.

## Ejecutar en el entorno de nube preparado

```bash
cd /workspace/ps4/PS4-FrameGen
source scripts/cloud-env.sh
bash scripts/build-host.sh
build/framegen_cli --demo --multiplier 2 --process 720 --output 720 --output-dir artifacts/demo
```

`artifacts/demo` contiene los dos originales, las imágenes intermedias y sus referencias analíticas,
un raster del menú y `metrics.json`. Esta escena es una textura analítica con movimiento conocido;
se identifica como tal. No representa un juego capturado ni pretende demostrar FPS de pantalla.

Para imágenes propias, convertir a PPM binario RGB de 8 bits, sin cambiar silenciosamente dimensiones:

```bash
ffmpeg -i original_a.png -frames:v 1 a.ppm
ffmpeg -i original_b.png -frames:v 1 b.ppm
build/framegen_cli --input a.ppm --next b.ppm --multiplier 4 --output 720 --output-dir artifacts/own
```

Opciones: `--process native|1080|720`, `--output native|1080|720`,
`--profile performance|quality`, `--filter area|bilinear`, `--repeats 3`, `--disabled`.
`--truth` puede repetirse para proporcionar una imagen retenida por cada posición temporal generada.
El CLI guarda A, los intermedios y B una sola vez. En un corte detectado no genera intermedios.

## Pruebas reproducibles y resultados

```bash
bash scripts/fetch-test-video.sh
python3 scripts/benchmark.py --video data/opencv-1920x1080.avi --output artifacts/validated --repeats 3
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DFRAMEGEN_SANITIZE=ON
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure
```

El vídeo tiene 1920×1080 y 25 imágenes originales/s. El script comprueba que las cinco marcas de
tiempo consecutivas son equidistantes. Usa originales 0 y M como entrada y reserva las imágenes
1…M−1 para comparar calidad. La cadencia de originales del par es 25/M, sin presentaciones físicas.
Los resultados no se convierten a FPS de pantalla. Entrada sintética adicional: 2560×1440.

Ver [auditoría](docs/FEASIBILITY.md), [mediciones](docs/RESULTS.md), [arquitectura](docs/ARCHITECTURE.md)
y [registro de entrega y pendientes](docs/STATUS.md). Las imágenes grandes permanecen en `artifacts/`,
ignoradas por Git; los informes y la receta de regeneración forman parte del proyecto.

## Windows

Necesita Visual Studio Build Tools con «Desktop development with C++», CMake 3.16+ y, para el
benchmark de vídeo, Python 3 y FFmpeg en PATH. Ejecutar en una consola de desarrollo x64:

```powershell
pwsh -File scripts/audit-windows.ps1
pwsh -File scripts/build-windows.ps1
.\build\Release\framegen_cli.exe --demo --multiplier 3 --output 720 --output-dir artifacts\demo
```

El script de auditoría comprueba presencia de herramientas sin leer secretos. Estas instrucciones
están preparadas para Windows; no se ejecutaron en un equipo Windows en esta sesión.
Para generar PKG desde Windows se recomienda WSL con LLVM/OpenOrbis y `scripts/build-ps4.sh`.
La receta de nube usa paquetes Debian 13 y depende de las herramientas base de esa imagen.

## Compilar la aplicación PS4

```bash
source scripts/cloud-env.sh
bash scripts/build-ps4.sh
```

Salida: `build-ps4/IV0000-BREWFG001_00-PS4FRAMEGEN00001.pkg`, además de ELF/OELF/eboot.
El script falla si faltan bibliotecas o herramientas y ejecuta `pkg_validate` al terminar.
Las firmas son las del formato homebrew/fake PKG, no una firma comercial de Sony.
Se usa OpenOrbis 0.5.4 oficial, Clang/LLD 19.1.7 y las bibliotecas PS4 del SDK.
La dependencia OpenSSL 1.1 de PkgTool se extrae de un paquete Debian autenticado y se utiliza
solo en el proceso de empaquetado. No se modifica la configuración TLS del equipo ni de APT.

Para reconstruir herramientas y ambos destinos en la nube: `bash scripts/setup-cloud.sh`.
El tarball se fija por SHA-256 y los paquetes APT por versión; las versiones antiguas deberán
seguir disponibles o conservarse en la instantánea/caché preparada.

## Probar en consola cuando se confirme compatibilidad

El paquete es experimental y no se ha instalado. Una vez verificados firmware y versión completa
de GoldHEN, instalarlo mediante el instalador de paquetes del entorno homebrew existente.
No actualizar firmware ni cargarlo como plugin en un juego. Abrir la aplicación propia.

- El motor empieza desactivado y la escena original usa una superficie 1920×1080.
- L3+R3 abre/cierra; cruceta selecciona/modifica; Cruz cambia; Círculo cierra el menú; Options sale.
- Ajustar multiplicador, perfil, salida y procesamiento en el menú. Cruz en ENGINE activa/desactiva.
- 1080p y nativa coinciden en esta aplicación porque su entrada nativa es 1920×1080.
- La salida 720p registra una superficie de 1280×720. El modo HDMI físico se registra por separado
  y puede seguir siendo 1080p por el escalador del sistema; no se cambia globalmente ese modo.
- Las preferencias viven en `/data/PS4FrameGen/BREWFG001.cfg`. Nunca guardan activación automática.
  Un archivo inválido usa valores predeterminados y emite un error. Desactivar restaura la superficie
  original; un fallo del controlador de vídeo detiene el programa si no puede recuperarse.

Seguir [el protocolo de hardware](docs/HARDWARE_TEST.md) antes de calificar la aplicación como funcional
en PS4. No hace falta instalar ni deshabilitar un plugin: no se distribuye ninguno en esta entrega.
