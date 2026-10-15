# PS4 FrameGen: auditoría de arranque, serie BOOT01–BOOT04

Fecha: 15 de octubre de 2026 (UTC−4). Estado: **pendiente de prueba física**.

## Resultado y alcance

El usuario probó v0.3 en PS4 Fat, firmware declarado 13.52 y GoldHEN declarado
«beta 12»: instala, aparece en el menú y al abrir muestra «Los datos de la
siguiente aplicación están dañados». No alcanza la interfaz. El usuario confirma
que no aparece código CE. No hay registro de carga ni consola conectada a este
entorno. Tampoco está confirmado el arranque de otro homebrew en esa sesión.

Se encontró y corrigió en un enlace nuevo un defecto verificable del ejecutable:
ocho bytes de BSS quedan fuera del tamaño de segmento declarado después de la
conversión Orbis. **No está confirmado que ese defecto provoque el aviso de la
consola**. Los ocho bytes están dentro de la misma página de 16 KiB; el cargador
podría redondear el mapeo. La corrección está comprobada estáticamente, no en PS4.

Se entregan cuatro PKG progresivos con identidades independientes. No se publica
un reemplazo de FrameGen como «corregido»: primero hay que demostrar el arranque
con estos controles. No se han añadido funciones de generación de fotogramas.

## PKG v0.3 publicado

Archivo auditado: `IV0000-FGEN00003_00-PS4FRAMEGEN00003.pkg`, 6.619.136 bytes.
SHA-256: `68db8a909882f4eb2e4c2dea5fafa6af61adc8215c266a120a2891491cab27bc`.
Es el archivo publicado en GitHub. No se dispone del hash de la copia transferida
a la consola, por lo que no se puede excluir un problema de transferencia.

| Elemento | Resultado observado |
| --- | --- |
| Empaquetado | GP4 `pkg_ps4_app`, aplicación completa/digital, fake PKG de OpenOrbis/LibOrbisPkg |
| Integridad | Todos los hashes y firmas de depuración comprobados por `pkg_validate`: OK |
| Title ID / Content ID | `FGEN00003` / `IV0000-FGEN00003_00-PS4FRAMEGEN00003`, coherentes |
| SFO real extraído del PKG | Conserva los valores del SFO fuente; `CATEGORY=gd`, `APP_TYPE=1`, `APP_VER=VERSION=01.02` |
| Versión mínima declarada | `SYSTEM_VER=0`; no constituye una prueba de compatibilidad con ningún firmware |
| Archivos de aplicación | `eboot.bin` y `framegen_bridge.prx`, idénticos a los archivos compilados; PFS incluye su `keystone` |
| Metadatos | `param.sfo` está en las entradas de metadatos; el empaquetador añade `PUBTOOLINFO`/`PUBTOOLVER` |
| Icono | No contiene `icon0.png`; no se ha demostrado que explique el fallo. Los diagnósticos sí llevan icono propio |
| Rutas | Ejecutable en la raíz, metadatos en las entradas correspondientes; no se detectan discrepancias con GP4 |

El PRX está presente en v0.3, pero su carga en la aplicación exige habilitación
explícita. Que esté empaquetado no demuestra que llegase a cargarse en la consola.
Las firmas fake válidas en la herramienta no prueban que GoldHEN las acepte.

Evidencia: [auditoría JSON](published-v03/audit.json),
[validación del paquete](published-v03/pkg-validate.txt),
[entradas](published-v03/pkg-entries.txt) y [extracción](published-v03/pkg-extract.txt).

## Ejecutable: causas confirmadas y posibilidades

### Defecto confirmado de cobertura BSS

El ELF x86-64 original tiene `.bss` en `0x148340`, tamaño `0x3670`, final
`0x14b9b0`. Después de `create-fself`, el segmento RW comienza en `0x148000`
y declara `p_memsz=0x39a8`: termina en `0x14b9a8`, ocho bytes antes.

El código del conversor suma el tamaño de BSS redondeado a 16 al tamaño de datos
inicializados, sin incluir el hueco de alineación previo a BSS. El script de
enlace usado por v0.3 recoge `.data` sin reunir `.data.*`, permitiendo subsecciones
huérfanas y ese hueco. La nueva variante local reúne los datos y los rellena antes
de una BSS alineada. Se han reenlazado **los objetos originales de v0.3**, sin
recompilar el motor, y la comprobación de cobertura pasa.

### Cambio confirmado de alineación TLS

El ELF original declara TLS con alineación de 8 bytes; el conversor la cambia a
32. El script original tampoco recoge por patrón `.tbss.*`. El nuevo enlace
recoge todas esas subsecciones y las alinea/rellena a 32 desde el principio.
Desaparece la diferencia entre entrada y salida. Su efecto en esta consola
necesita prueba de ejecución; no se atribuye a ello el aviso sin evidencia.

### Comprobaciones restantes

- ELF de entrada: x86-64, little endian, PIE para FreeBSD; salida Orbis
  `ET_SCE_EXEC_ASLR` (`0xfe10`), ABI FreeBSD. Entrada `_start` del CRT OpenOrbis
  conservada, dentro de memoria ejecutable. Parámetros de proceso `ORBI` presentes.
- Fake SELF: tamaño, cabeceras incrustadas y segmentos coinciden con el OELF.
  PAID `0x3800000000000011`, tipo 1, como la receta oficial de OpenOrbis.
- Importaciones reales: `libkernel.prx`, `libSceVideoOut.prx`, `libScePad.prx` y
  `libSceUserService.prx`. `libc` y `libc++` se enlazan estáticamente. Se revisan
  NID, límites de reubicaciones y tablas SCE, sin afirmar disponibilidad en 13.52.
- El `readelf -d` genérico muestra nombres incorrectos en el OELF si usa la antigua
  `.dynstr`. El inspector nuevo lee `DT_SCE_STRTAB` en `PT_SCE_DYNLIBDATA`; no se
  considera `pthread_create` una biblioteca faltante.
- La versión anterior realiza acceso a configuración/diagnósticos y construye
  componentes CPU antes de presentar su interfaz. Esas operaciones pueden fallar
  temprano. Los controles nuevos aíslan esas etapas, permiten continuar si falla
  el log opcional y no cargan ningún PRX.

Siguen abiertas: rechazo del cargador/entorno GoldHEN, copia dañada en el medio,
incompatibilidad de alguna importación o fallo temprano del runtime. Sin código CE,
log o ejecución de un control mínimo no hay base para elegir una causa definitiva.

## GoldHEN y firmware declarado

Se consultaron las [publicaciones oficiales de GoldHEN](https://github.com/GoldHEN/GoldHEN/releases)
y [v2.4b18](https://github.com/GoldHEN/GoldHEN/releases/tag/2.4b18).
La lista visible de v2.4b18 menciona 5.05, 6.71/6.72, 9.00, 9.60,
10.00/10.01, 10.50, 10.70/10.71 y 11.00. No menciona 13.52.
«beta 12» por sí solo no identifica toda la versión. Esto no confirma ni descarta
el entorno que el usuario describe; **no hay prueba de compatibilidad de estos
binarios con 13.52 / beta 12**. La instalación que el usuario sí comprobó no
demuestra que el ejecutable arranque.

No se cambia firmware, archivos del sistema ni configuración de plugins.
El [texto visible consultado](goldhen-release-visible.txt) queda como evidencia.

## Controles entregados y validación

| PKG | Componentes | Resultado esperado que debe observar el usuario |
| --- | --- | --- |
| BOOT01, `FGBT00001` | C, CRT OpenOrbis y solo `libkernel`; sin gráficos, mando, C++, hilos, motor ni archivos de log | Notificación «MAIN alcanzado»; espera de 20 s y retorno |
| BOOT02, `FGBT00002` | Añade memoria directa y VideoOut; sin C++, mando ni motor | `02` verde en fondo azul, primer flip confirmado, salida en 30 s |
| BOOT03, `FGBT00003` | Añade C++, `Image`, texto y mando | Imagen de diagnóstico con BOOT03, FG OFF, OPTIONS para salir o 30 s |
| BOOT04, `FGBT00004` | Añade `Processor`, pool de dos hilos y paso del original con `enabled=false` | Imagen BOOT04, cero fotogramas generados, OPTIONS o 30 s |

No hay controles simulados: OPTIONS está conectado solo en BOOT03/04; no se ofrece
L3+R3, activación de FG, captura de juegos ni presentación dentro de juegos.
Los cuatro PKG excluyen el PRX y no necesitan texturas, fuentes o configuración
externas. BOOT02/03/04 intentan escribir únicamente su log opcional propio.

Los cuatro ejecutables pasan las comprobaciones estáticas de formato, entrada,
cobertura, TLS, dependencias y contenido SELF. Los cuatro PKG pasan los hashes y
firmas fake de `pkg_validate`; sus ejecutables extraídos coinciden byte a byte y
sus metadatos conservan la identidad esperada. Cada PKG ocupa 6.619.136 bytes por
la estructura del empaquetado, aunque el SELF mínimo solo ocupa 102.064 bytes.

La suite de regresión final ejecutó **12 pruebas, todas correctas**. Incluye
mutaciones negativas de entrada, ELF truncado, cobertura, SELF y SFO; detecta el
defecto de v0.3 y acepta el reenlace; comprueba dependencias por etapa e invariancia
de versiones anteriores. [Salida completa](regression-tests-final.txt).

Entorno de compilación: Linux x86-64, Clang/LLD 19.1.7, OpenOrbis 0.5.4,
sin SDK propietario ni PS4/emulador. Las etapas están compiladas para consola;
ninguna se ha ejecutado en consola desde este entorno. No se han medido tiempos
de Frame Generation nuevos porque esta entrega se limita al arranque.

## Conservación y siguiente criterio

Los manifiestos históricos v0.1, v0.2 y v0.3 y los 29 archivos de distribución
registrados antes de esta auditoría conservan sus SHA-256. No se modifican
los PKG, PRX, resultados o ZIP históricos. Se guarda el
[inventario previo](preservation/preserved-sha256.json) y el
[informe físico del usuario](preservation/console-report.json).

Primero debe ejecutarse BOOT01 y, si llega a `main`, continuar en orden.
El primer control que falle delimitará la siguiente corrección. Hasta entonces
no se declara resuelto el error ni se emite una versión completa como compatible.
Véase [prueba y recuperación](PRUEBA_PS4.md).

## Referencias técnicas

- [Conversor oficial, cálculo de segmentos](https://github.com/OpenOrbis/create-fself/blob/f2da6229684ce320f673c06fceac9c218f621788/pkg/oelf/OELFGenProgramHeaders.go).
- [OpenOrbis, script original](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/6929bafac0b565692ed61e66032a79185ee9cf65/link.x).
- [Código de diagnóstico y reproducción](../../boot-diagnostics/README.md).
