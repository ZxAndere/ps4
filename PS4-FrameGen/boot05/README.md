# BOOT05: control de los módulos de paquete OpenOrbis

**No está probado físicamente.** Hipótesis: BOOT01 omite dos módulos de
compatibilidad que la documentación y los ejemplos de OpenOrbis incluyen en el
paquete. BOOT05 los añade conservando el ejecutable de BOOT01 byte a byte.
No activa FrameGen, vídeo, mando ni plugins propios.

La aplicación instalada se llama **FrameGen BOOT05 MODULE CONTROL**, ID
`FGBT00005`. Si entra al programa, su notificación seguirá diciendo
**«FrameGen BOOT01: MAIN alcanzado»**: se conserva intencionalmente el binario
original. El icono también es idéntico al de BOOT01. No confundir el nombre de la
aplicación instalada con el texto interno del control.

Código del programa empaquetado: [boot01.c](../boot-diagnostics/boot01.c),
[platform.c](../boot-diagnostics/platform.c) y
[platform.h](../boot-diagnostics/platform.h). No hay una implementación nueva
oculta de `main`. El nuevo código de BOOT05 es el constructor diferencial
`build.py`, el auditor `audit.py` y sus pruebas.

## Construcción revisada

Requiere las herramientas ya preparadas: OpenOrbis 0.5.4 oficial, Clang/LLD
19.1.7 y el runtime aislado de PkgTool. El ZIP oficial del SDK se verifica con
SHA-256 `3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526`.

**No ejecutes `scripts/setup-cloud.sh` sobre los builds históricos:** su secuencia
termina llamando al constructor antiguo con el destino predeterminado. Para esta
prueba solo se activa el entorno existente y se usa un destino nuevo:

```bash
cd PS4-FrameGen
source scripts/cloud-env.sh
python3 boot05/build.py --output build-boot05-local-01
FRAMEGEN_BOOT05_BUILD_DIR="$PWD/build-boot05-local-01" python3 boot05/tests.py
```

El destino debe no existir. La carpeta publicada se construyó como
`build-boot05-03`; no se sobrescribe para repetir la prueba. Un checkout completo
del repositorio conserva las referencias históricas que exige la prueba de
preservación. El ZIP específico de BOOT05 incluye sus fuentes, las fuentes del
control, informes y el PKG BOOT01 usado como entrada; no contiene todos los
archivos históricos necesarios para esa prueba de preservación.

El constructor:

1. Verifica y extrae el BOOT01 publicado, en vez de confiar en una carpeta de build.
2. Recompila sus fuentes y exige el mismo SHA-256 del ELF original:
   `72b66a17839972e7b1150b2645d74082a2e14af13efbc8079df9303bb5fb6a27`.
3. Audita una nueva conversión. `create-fself` incrusta la ruta completa de entrada
   en `DT_SCE_FILENAME`, por lo que el SELF convertido en otra carpeta cambia.
   Para conservar el control exacto, empaqueta el SELF extraído y verificado de
   BOOT01, SHA-256 `ad6cdb3be767681159c99d52c3cfdac6195f1c6323d158886253f539c2a64263`.
4. Añade únicamente `sce_module/libc.prx` y `sce_module/libSceFios2.prx` del SDK
   oficial, con hashes fijados. Son reemplazos abiertos de OpenOrbis; no son el
   puente PRX de FrameGen ni módulos propietarios copiados de una consola.
5. Cambia solo TITLE, TITLE_ID y CONTENT_ID del SFO fuente. Conserva los demás
   valores, icono, PAID, tipo SELF y ausencia de authinfo adicional.
6. Construye el fake PKG con GP4, valida hashes/firmas fake, extrae el resultado y
   verifica identidad, ejecutable y módulos. Rechaza archivos adicionales.

La nueva conversión y sus logs se conservan para comparar; **no** se usa su SELF
en el PKG. `executable-audit.json` audita esa conversión reconstruida;
`self-chain.json` audita los tres SELF que realmente se empaquetan. El manifiesto
indica explícitamente el origen del ejecutable del paquete.

## Referencia oficial reproducida

Se copió `samples/hello_world` y `samples/_common` del SDK a
`build-boot05-01/reference/samples/`. Sobre esa copia se ejecutó:

```bash
source scripts/cloud-env.sh
DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
DOTNET_BUNDLE_EXTRACT_BASE_DIR="$PWD/build-boot05-01/reference/dotnet" \
LD_LIBRARY_PATH="$FRAMEGEN_PKG_LIBDIR" \
make -C build-boot05-01/reference/samples/hello_world \
  CC="$FRAMEGEN_PS4_CXX" CCX="$FRAMEGEN_PS4_CXX" LD="$FRAMEGEN_PS4_LD"
```

No se modificaron sus fuentes, Makefile, CRT ni módulos. El ejemplo se compiló
y validó localmente; no se ejecutó en consola. Su PKG no se distribuye como prueba
adicional para el usuario ni se solicita continuar con BOOT02–04.

## Evidencia y licencia

- [Diagnóstico y diferencias](../docs/boot05/DIAGNOSTICO.md).
- [Prueba física y recuperación](../docs/boot05/PRUEBA_PS4.md).
- [Resultados de pruebas](../docs/boot05/tests.txt).
- [Descargas](../../downloads/boot05/README.md).

El código nuevo se distribuye bajo GPL-3.0-or-later; se conserva la licencia
[incluida con el código de diagnóstico](../boot-diagnostics/LICENSE-OpenOrbis.txt).
Las citas y fuentes del SDK se conservan con su LICENSE y las de musl con COPYRIGHT.
Las fuentes de comparación de PS4 Cheats Manager conservan su licencia propia;
no se incorpora su ejecutable ni se copian sus permisos de authinfo.
