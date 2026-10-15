# Diagnósticos de arranque PS4

Código de la serie `boot-audit-01`. Sus cuatro PKG aún requieren ejecución física.
Consulta el [informe](../docs/boot-audit/DIAGNOSTICO.md) y las
[instrucciones para PS4](../docs/boot-audit/PRUEBA_PS4.md).

## Compilar sin sobrescribir versiones

Desde `PS4-FrameGen`, usa las dependencias y activación existentes:

```bash
bash scripts/setup-cloud.sh
source scripts/cloud-env.sh
python3 boot-diagnostics/build.py --output build-boot-local-01
```

El instalador existente prepara OpenOrbis completo 0.5.4, Clang/LLD 19 y el runtime
aislado del empaquetador. No requiere modificar el SDK ni archivos de consola.
`--output` debe ser un directorio que no exista; el constructor rechaza sobrescribir
una compilación. Para repetir, escoge otro nombre. No ejecutes el script histórico
de v0.3 con su destino predeterminado sobre los originales preservados.

El constructor compila C/C++ para `x86_64-pc-freebsd12-elf`, utiliza el CRT oficial,
`link-boot.x`, conversión ELF → OELF/fake SELF y un GP4 de aplicación completa.
Después valida los cuatro PKG, los extrae y compara el payload y metadatos.
No carga PRX ni habilita FG. Los comandos exactos quedan en `bootNN/build.txt`.
Los logs `pkg-validate.txt`, `pkg-extract.txt`, `packaged-sfo.json` y
`executable-audit.json` son la evidencia de cada etapa.

`inspect_orbis.py ELF OELF SELF NUEVO_REPORTE.json` puede auditar una conversión
por separado. No certifica comportamiento del cargador ni disponibilidad de NID
en firmwares concretos. `--expect-invalid` permite comprobar el defecto conocido
en los binarios históricos sin confundirlo con un resultado correcto.

## Reproducir la auditoría histórica

La nube conserva `build-ps4-v03-final` y sus objetos originales. El ZIP histórico
v0.3 publicado conserva fuentes y ELF de referencia, pero no incluye todos los
objetos ni el OELF; un checkout de Git por sí solo tampoco contiene esos builds.
Las regresiones históricas necesitan esas referencias, los artefactos originales
y los manifiestos conservados, además de la nueva serie compilada.

En el entorno conservado se ejecutó:

```bash
source scripts/cloud-env.sh
python3 boot-diagnostics/relink_v03.py \
  --objects-build build-ps4-v03-final \
  --output build-boot-audit-02/relinked-v03-verified
FRAMEGEN_BOOT_RELINK_DIR="$PWD/build-boot-audit-02/relinked-v03-verified" \
  python3 boot-diagnostics/tests.py
```

El reenlace no construye ni publica un PKG completo. Para otros directorios, las
pruebas aceptan `FRAMEGEN_BOOT_BUILD_DIR`, `FRAMEGEN_V03_BUILD_DIR` y
`FRAMEGEN_BOOT_RELINK_DIR`. Su ruta de preservación predeterminada es
`artifacts/boot-recovery-v03/preserved-sha256.json`; esa instantánea se guarda también
en los informes. Las 12 pruebas no requieren conexión con la consola.

Para repetir la extracción/auditoría del PKG antiguo sin sobrescribir informes:

```bash
python3 boot-diagnostics/audit_existing.py \
  --original-build build-ps4-v03-final --output build-v03-pkg-audit-local-01
```

El nuevo script no modifica fuentes ni builds antiguos. Para recompilar los objetos
históricos desde una copia sin ellos, el script existente admite
`FRAMEGEN_PS4_BUILD_DIR="$PWD/build-v03-reference-local-01" bash scripts/build-ps4.sh`.
Ese comando reproduce la receta antigua en **otro** directorio: su PKG de referencia
no debe presentarse como una corrección. No sustituye la prueba sobre el binario
publicado. Las comparaciones contra el defecto original usan los binarios originales.

## Distribución

Los PKG y el ZIP de fuentes están en [descargas](../../downloads/boot-audit-01/README.md).
El ZIP de fuentes contiene el código del proyecto, diagnósticos e informes
seleccionados; no incluye el SDK ni todos los builds históricos. Los originales
siguen disponibles en el ZIP v0.3 y conservados en la nube. Véase [licencias](NOTICE.md).
