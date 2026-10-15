# Comprobación del paquete 0.2

Paquete nuevo: `build-ps4-v02-final/IV0000-FGEN00001_00-PS4FRAMEGEN00001.pkg`.
El `.pkg` 0.1 permanece en su ubicación anterior y mantiene su SHA-256 original.

Se utiliza `FGEN00001` como identificador nuevo de cuatro letras y cinco cifras, con un título que
identifica la versión 0.2. Esto separa la nueva aplicación del paquete experimental anterior.
Las preferencias nuevas se guardan en `/data/PS4FrameGen/FGEN00001.cfg`; si falta ese archivo,
la app puede leer `BREWFG001.cfg` y guardar después un archivo nuevo. No borra el anterior.
Este ajuste sigue una convención de identificador y no prueba por sí mismo instalación en firmware alguno.

## Qué se comprobó con herramientas

- Compilación cruzada de la aplicación y el módulo con Clang/LLD 19.1.7 y OpenOrbis 0.5.4.
- Compilación de SIMD SSE2 y de la implementación de hilos, con imports resueltos por el SDK.
- ELF x86-64 con dependencias de Orbis: kernel, VideoOut, Pad y UserService. El PRX depende de kernel;
  no necesita bibliotecas Linux del host para el destino.
- Conversión real a fake SELF de `eboot.bin` y `framegen_bridge.prx`.
- Metadatos SFO y coherencia de la identidad del paquete seleccionado.
- `PkgTool.Core pkg_validate`: hashes y firmas estructurales correctos del formato homebrew.
- Extracción del PKG: `eboot.bin` y `framegen_bridge.prx` coinciden byte a byte con los binarios compilados.
- Exportaciones dinámicas del PRX: API, `plugin_load`, `plugin_unload` y metadatos.
- Manifiesto de artefactos 0.1: todos sus hashes siguen coincidiendo.

El control de exportaciones detectó una primera construcción sin `fg_bridge_api` en la tabla
dinámica. Se corrigió el enlace con una lista explícita de exportaciones. La validación posterior
comprueba ese requisito, además de los hashes del contenedor; un PKG íntegro puede contener un
programa con errores y por eso no se confunden esas dos verificaciones.

```bash
cd /workspace/ps4/PS4-FrameGen
source scripts/cloud-env.sh
bash scripts/build-ps4.sh
# El verificador rechaza sobrescribir un informe previo: elegir una ruta nueva para otra ejecución.
python3 scripts/verify-ps4-artifacts.py --output artifacts/optimization-v02/pkg-check-new
```

## Qué sigue sin comprobar

Instalación, arranque, creación de hilos, aceptación de superficies, colores, cambio de resolución,
sincronización de flips, carga/descarga del PRX y estabilidad en una PS4. No se ha ejecutado el ELF
Orbis como si fuera un programa Linux: ese host no ofrece el kernel ni las bibliotecas de consola.

Firmware **13.52** y GoldHEN **«beta 12»** siguen siendo datos aportados por el usuario, sin verificación
en hardware ni identificación completa de la versión. No se declara compatibilidad. Para resolverlo
se necesita una consola accesible y los registros reales de instalación/arranque siguiendo
`HARDWARE_TEST.md`, primero con el motor integrado desactivado y después con el PRX cooperativo.
No se actualizará firmware ni se modificarán archivos del sistema para realizar esa prueba.
