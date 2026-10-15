# GPU y GoldHEN 0.3: evidencia y límites

## GPU

`artifacts/v03/capabilities.json` registra probes de solo lectura: no hay `/dev/dri`, emulador Orbis
ni endpoint PS4 configurado. No hay ejecución GPU ni cifras CPU/GPU comparables. La GPU GCN de PS4
puede hacer cómputo; esa propiedad del hardware no acredita un backend disponible en este proyecto.

Fuentes inspeccionadas:

- [OpenOrbis GnmDriver.h](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/6929bafac0b565692ed61e66032a79185ee9cf65/include/orbis/GnmDriver.h).
  El SDK instalado declara `void sceGnmDispatchDirect()` y `void sceGnmMapComputeQueue()` sin
  argumentos descritos. Hay funciones de envío de command buffers, pero falta una ruta completa
  verificada de comandos, descriptores, shaders propios, fences y coherencia de recursos.
- [PigletApplication.cpp](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/6929bafac0b565692ed61e66032a79185ee9cf65/samples/piglet/piglet/PigletApplication.cpp),
  líneas 36–83 y 384–396: carga `libScePigletv2VSH.sprx`/`libScePrecompiledShaders.sprx`, localiza
  binarios existentes y usa `glShaderBinary`, formato PSSL. La declaración `glShaderSource` en
  GLES2 y las cabeceras SDL de cómputo no demuestran un compilador/runtime disponible en esta PS4.

No se han descargado módulos propietarios ni cambiado archivos del sistema. No se añade un backend
vacío. La siguiente prueba GPU requiere ejecutar un escalador sencillo en la aplicación propia con
una biblioteca y toolchain de shaders demostrados, validar salida/canales/pitch, esperar fences y medir
transferencias, cómputo y sincronización juntos. La memoria unificada de PS4 no elimina los problemas
de layout tiled/lineal, visibilidad, coherencia ni propiedad de superficies. Tampoco permite asumir
recursos libres mientras un juego utiliza la GPU. Un objeto HSA de PC no es por sí solo un shader PS4.

## Puente cooperativo PRX

ABI `0x00030000`: los consumidores ABI 2 deben continuar con el PRX 0.2 preservado. La nueva aplicación
consulta la versión y tamaño antes de usar la tabla. `game_hooks=0`, `gpu_backend=0`.

| Función | Evidencia |
|---|---|
| Copia validada de RGBA8 lineal con pitch/capacidad y vida explícita | Ejecutada en integración host |
| Interpolación real, menú L3+R3, configuración, reentrada Busy y errores | Ejecutados en host |
| Callback offline, callback de confirmación y descarte del consumidor | Contratos probados con callbacks de prueba; no hardware |
| Deadline de captura y bypass de un par fuera de presupuesto | Probados en host con reloj steady real |
| Carga PRX por la aplicación propia, mando y callback VideoOut | Compilados; sin ejecutar en PS4 |
| Captura/overlay/inserción dentro de un juego externo | No implementados ni probados |

`FGInput` exige `CPU_READY | RETAIN_UNTIL_RETURN`; no acepta superficies GPU que todavía se estén
renderizando. Un consumidor offline recibe imágenes y nunca suma flips. `FG_CONFIRMED_FLIP` requiere
un timestamp monótono de confirmación; `FG_DROPPED_BY_CONSUMER` solo es válido para generados.
Con `FG_CLOCK_MONOTONIC`, captura y motor deben usar el mismo reloj; la app espera los tiempos de
`FGOutput`, vuelve a comprobar caducidad y solo confirma tras el flip. Sin ese flag no se inventan
deadlines para timestamps de archivos. `source_budget_us=0` usa el intervalo observado; la aplicación
propia declara 33333 µs para que su bucle síncrono no oculte sobrecarga. El lookahead estimado conserva
el intervalo de captura realmente observado. La copia de captura y VideoOut no forman parte del kernel;
`total_bridge_ms` incluye el trabajo cooperativo y callbacks.

La restauración de perfiles es una operación de la aplicación propia y no aparece como control en el
puente. En su ruta cooperativa la app exporta JSONL a partir del snapshot y de sus confirmaciones.
No se cambia la configuración global de plugins GoldHEN.

## Investigación de hooks, sin atribuirles captura universal

Repositorio [GoldHEN Plugins](https://github.com/GoldHEN/GoldHEN_Plugins_Repository/tree/998fe03fe0275b59310511187ba8550ed475a003):
`gamepad_helper` usa hooks de Pad; `frame_logger` intercepta `sceGnmSubmitAndFlipCommandBuffers`
para registrar tiempos; `fliprate_remover` modifica límites, no genera imágenes. Esos ejemplos dan
puntos de instrumentación, no propiedad de buffers ni permiso para escribir mientras la GPU renderiza.
`force_1080p_display` cambia la información reportada y no constituye escalado real.
SDK inspeccionado: `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7`.

Una adaptación por juego todavía necesita identificar registro/formato de superficies, adquirirlas
tras fences, convertir layout si procede, mantener un buffer de salida propio, sincronizar la inserción
sin devolver índices ajenos al renderizador y revertir hooks con seguridad. El juego necesita recursos
CPU/GPU y margen de presentación suficientes. El PRX actual no instala esos hooks.

Las notas oficiales históricas consultadas para GoldHEN no verifican 13.52, y «beta 12» no identifica
una versión completa. No se deduce ni compatibilidad ni imposibilidad a partir de ese dato.
