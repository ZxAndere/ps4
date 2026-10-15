# BOOT01 rechazado en consola: auditoría y prueba BOOT05

15 de octubre de 2026 (UTC−4). **Causa física sin confirmar; BOOT05 sin ejecutar
en consola.** No se declara solucionado el problema ni compatibilidad con 13.52
o GoldHEN «beta 12».

## Evidencia física nueva

El usuario confirma que BOOT01 muestra el mismo aviso de datos dañados y no se
observa «MAIN alcanzado». Detuvo la serie. También confirma que **PS4 Cheat
Manager abre en esa misma sesión**. No conocemos la versión exacta de ese PKG,
el hash de la copia de BOOT01 transferida a la consola ni un código/log del cargador.

Por tanto, la corrección BSS/TLS de la entrega anterior no bastó para conseguir
el arranque. No hay motivo para probar BOOT02–04 ni añadir componentes del motor.
Que otro homebrew abra reduce la hipótesis de un rechazo general de homebrew,
pero no valida todas las importaciones, SELF y recetas de empaquetado.

No observar la notificación **no demuestra que no se ejecutara `main`**.
BOOT01 primero llama a `boot_mark`, que llama a `sceKernelDebugOutText`, y solo
después envía la notificación. Puede fallar antes del CRT, dentro del CRT o en
esas primeras llamadas. Sin depuración del cargador/entrada no se puede localizar
el corte con precisión. El aviso de la shell tampoco distingue esas posibilidades.

## Comparación concreta con OpenOrbis y PS4 Cheats Manager

Se obtuvo del SDK oficial 0.5.4 el ejemplo **`samples/hello_world`**, con fuente,
README y Makefile documentado. Se compiló sin cambios en una copia independiente
con Clang/LLD 19.1.7 y las bibliotecas del SDK; su PKG pasó `pkg_validate` y su
ejecutable pasó la auditoría estática, con la advertencia TLS de la receta original.
No se ha ejecutado ese ejemplo en PS4. [Comandos y log](official-hello-world/build.txt).

La receta pública actual de [bucanero/PS4CheatsManager](https://github.com/bucanero/PS4CheatsManager/tree/248e8f23538b4e57e7ec73ac4bd2eca96f2180b5)
usa también OpenOrbis. Se compara su **fuente actual**, sin asumir que corresponda
exactamente al binario instalado por el usuario.

| Aspecto | BOOT01 | `hello_world` oficial | PS4 Cheats Manager, fuente consultada |
| --- | --- | --- | --- |
| Destino | x86-64 FreeBSD 12 ELF, convertido a Orbis | Igual | Igual |
| CRT | `lib/crt1.o`, musl | Igual SDK | Ruta `lib/crt1.o`; revisión instalada desconocida |
| Enlace | Script local revisado, entrada explícita, GC y páginas 16 KiB | `link.x` oficial, sin esas opciones adicionales | `link.x` del SDK que use su build |
| `sce_module/libc.prx` | **Ausente** | **Presente** | **Presente en el árbol Git** |
| `sce_module/libSceFios2.prx` | **Ausente** | **Presente** | **Presente en el árbol Git** |
| `sce_sys/about/right.sprx` | Ausente | Presente | Presente |
| PAID / tipo SELF | `0x3800000000000011` / fake 1 | Iguales | Mismo PAID; usa `--authinfo` adicional |
| SFO | `gd`, ATTRIBUTE 0, versión `01.00` | `gd`, ATTRIBUTE 0, versión `1.00` | `gde`, ATTRIBUTE 32, versión fuente `01.22` |
| PKG | GP4 `pkg_ps4_app`, full/digital50, LibOrbisPkg | Misma familia de herramientas/receta | Misma familia de herramientas/receta |
| `DT_SCE_FILENAME` | Ruta absoluta de compilación | Ruta relativa de compilación | La receta pasa ruta relativa; no se auditó el eboot instalado |

Los módulos del SDK y los del árbol actual de Cheats Manager tienen **hashes y
tamaños distintos**. No se presentan como el mismo binario. BOOT05 usa los del
SDK oficial 0.5.4 verificado, con fuente disponible y hashes fijados.

### Hipótesis que justifica BOOT05

El [README de fuentes de OpenOrbis](sources/OpenOrbis/src/README.md) dice:
“empty stub modules for `libc` and `libSceFios2`, which are needed in packages”.
Su [changelog v0.5](sources/OpenOrbis/CHANGELOG.md) prescribe colocarlos en
`sce_module/`; la nota histórica menciona Mira, por lo que no se extrapola una
garantía de comportamiento a GoldHEN. El ejemplo oficial y el homebrew ajeno
nombrado por el usuario los incluyen en sus recetas.

La omisión es una **diferencia confirmada y documentada del paquete**. Que sea
la causa del rechazo es una **hipótesis**, no un hecho. Como límite adicional,
el campo `Need_sceLibc` del CRT de BOOT01 vale **0** y `DT_NEEDED` de su ejecutable
solo lista `libkernel.prx`. No hay una importación directa de esos dos archivos
en su tabla dinámica. No se afirma que el cargador los haya solicitado: faltan
trazas físicas. BOOT05 comprueba si completar esa convención del paquete cambia
el resultado en el entorno real.

`right.sprx` está documentado como módulo de la shell para mostrar créditos desde
Información mediante una combinación de botones. No se ha encontrado evidencia
de que sea necesario para entrar a `main`; se mantiene fuera de BOOT05 para acotar
la prueba. Tampoco se copian authinfo, categoría `gde`, atributos ni permisos de
Cheats Manager, que tiene funciones muy distintas del control mínimo.

## Auditoría detallada de la cadena

**ELF y enlace.** Se recompilaron las fuentes originales de BOOT01 con sus opciones
revisadas. Ambos objetos y el ELF coinciden byte a byte con el build original.
ELF SHA-256: `72b66a17839972e7b1150b2645d74082a2e14af13efbc8079df9303bb5fb6a27`.
No se sustituyó CRT, compilador, biblioteca estática ni script sin registrar.

**Conversión.** ELF64 little endian, máquina AMD64, PIE de entrada; Orbis
`ET_SCE_EXEC_ASLR=0xfe10`, ABI FreeBSD al convertir. La entrada `0x1b8` apunta al
`_start` del SDK, que salta a `_start_ps4_c` y después a `__libc_start_main`.
El CRT procesa argumentos/entorno e inicializadores antes de llamar a `main`.

**Cabeceras y memoria de BOOT01.** Tiene nueve program headers. RX:
VA `0x0`, tamaño `0x8000`; SCE_RELRO: VA `0x8000`, tamaño `0xc000`;
RW: VA `0x14000`, tamaño de archivo `0x4000`, memoria `0x4050`.
TLS: VA `0x8360`, memoria `0x20`, alineación `0x20`.
Se verifican cobertura de secciones asignadas, páginas 16 KiB, reubicaciones,
entrada ejecutable y parámetros de proceso `ORBI` de tamaño `0x50`.
No se detecta el defecto BSS de v0.3 en BOOT01.

**Importaciones.** Su eboot conserva diez NID de `libkernel.prx`, correspondientes
a depuración, notificación, usleep y funciones que arrastra musl (`poll`, `_exit`,
`_open`, `_fcntl`, `close`, `__error`, `_ioctl`). No importa VideoOut, Pad ni motor.
`libc.a` es musl estática; los dos nuevos `*.prx` son stubs de compatibilidad del
paquete, no una sustitución de esa biblioteca estática. Estos stubs declaran
`libkernel.prx` y `libSceLibcInternal.prx`; los módulos del sistema no se copian ni
modifican. Su resolución real en la consola sigue pendiente.

**SELF.** Se decodificaron cabecera, extensión, tablas y segmentos reales.
Versión 0, modo 1, endian 1 (LSB), atributos `0x12`, KeyType `0x101` y flags
`0x22`, como el conversor oficial. El eboot tiene 8 entradas SELF y cada stub 10.
El eboot y los stubs tienen PAID `0x3800000000000011`, tipo fake 1; los stubs son
Orbis `0xfe18`. Se verifican límites, tamaños, correspondencia con program headers
y tablas SCE. BOOT01 y el ejemplo oficial no llevan authinfo adicional. Las firmas
fake requieren soporte del entorno; validarlas localmente no equivale a aceptación
por el cargador de la PS4.

**Metadatos y PKG.** BOOT01 instalado tiene identidad coherente `FGBT00001` /
`IV0000-FGBT00001_00-BOOTDIAG00000001`; APP_TYPE 1, CATEGORY `gd`, APP_VER/VERSION
`01.00`, SYSTEM_VER 0. Sus hashes y firmas fake validan y el eboot extraído coincide
con el publicado. En BOOT05 se comprueba también el SFO extraído del paquete real,
no solo el archivo fuente. `SYSTEM_VER=0` no acredita compatibilidad de firmware.

### Otra diferencia encontrada: ruta incrustada

El conversor oficial escribe la ruta completa que recibe en `DT_SCE_FILENAME`.
Al reconstruir en otra carpeta se obtuvo el mismo ELF pero un OELF/SELF distinto
por esa cadena y las posiciones de las tablas asociadas. La primera comprobación
de igualdad lo detectó y paró el constructor. Se conservó esa salida.

Para que BOOT05 pruebe únicamente los módulos ausentes, su eboot se extrae del
PKG BOOT01 publicado, se verifica por hash y se copia sin cambiar un byte.
La conversión nueva se conserva y audita como evidencia de reproducción; no es
el payload de BOOT05. La ruta absoluta frente a relativa queda registrada como
otra hipótesis posible, sin atribuirle el error ni cambiarla en esta prueba.

## Diagnóstico diferencial

| Posibilidad | Evidencia actual | Qué sigue sin saberse |
| --- | --- | --- |
| Rechazo al instalar el PKG | El usuario pudo instalar BOOT01; no es un rechazo de instalación | Puede haber comprobaciones de montaje/licencia/paquete durante la apertura |
| Rechazo/carga de SELF | Compatible con el aviso; formato estático y hashes pasan | Aceptación efectiva por el cargador, código de error y etapa precisa |
| Importaciones | Tablas coherentes; solo kernel en el eboot, con CRT musl | Disponibilidad/resolución real de cada NID y módulos implícitos |
| Entorno/firmware | Cheat Manager abre en la misma sesión | La versión exacta instalada y compatibilidad de esta receta con el entorno declarado |
| CRT/inicialización | Hay trabajo antes de `main`; no hubo notificación | Si se alcanzó `_start`, `__libc_start_main`, `main` o la primera llamada de depuración |
| Copia dañada | El PKG publicado tiene integridad correcta | Hash del archivo que se transfirió a la consola |
| Dos stubs omitidos | Diferencia documental y de contenido confirmada | Si añadirlos modifica el resultado físico: objetivo de BOOT05 |

La documentación oficial de GoldHEN consultada anteriormente no confirmaba
13.52 / «beta 12». El dato nuevo de Cheat Manager funcionando es evidencia real
del usuario, pero no permite certificar esta combinación para otro ejecutable.
No se pide cambiar firmware, cargar otro exploit ni tocar archivos críticos.

## Qué cambia exactamente en BOOT05

- Mismo `eboot.bin` que BOOT01, SHA-256
  `ad6cdb3be767681159c99d52c3cfdac6195f1c6323d158886253f539c2a64263`.
- Mismo icono, CRT, instrucciones, importaciones, linker, TLS, PAID y authinfo.
- Se añaden solo `sce_module/libc.prx` y `sce_module/libSceFios2.prx` del SDK.
- TITLE pasa a `FrameGen BOOT05 MODULE CONTROL`, TITLE_ID a `FGBT00005` y CONTENT_ID
  a `IV0000-FGBT00005_00-BOOTDIAG00000005` para preservar la instalación anterior.
  El empaquetador regenera sus hashes, firmas fake y metadatos derivados.
- No incluye `framegen_bridge.prx`, generación de fotogramas, gráficos ni mando.

PKG BOOT05: **6.619.136 bytes**. SHA-256:
`b50a67d5400eca1560531194cab50b0457440cfabd95c43ce41f2ea26281a527`.

Se ejecutaron **12 pruebas de BOOT05, todas correctas**, incluidas mutaciones que
quitan o cambian stubs, cambian el ejecutable, añaden payloads, alteran identidad
y dañan el SELF. Los 255 archivos anteriores de la instantánea —excluida la portada
del repositorio que se actualiza— conservan sus hashes. No se ejecutó ni reconstruyó
BOOT02–04. [Resultados](tests.txt), [comparación binaria](chain-comparison.json),
[manifiesto](boot05/manifest.json), [validación PKG](boot05/pkg-validate.txt).

El siguiente dato necesario es la ejecución física de **BOOT05 solamente**.
Si aparece «BOOT01: MAIN alcanzado» al abrir BOOT05, el control llegó a `main`.
Si vuelve el mismo aviso, esta adición no habrá bastado y habrá que investigar
las siguientes etapas con ese resultado, sin declarar falsamente una solución.
