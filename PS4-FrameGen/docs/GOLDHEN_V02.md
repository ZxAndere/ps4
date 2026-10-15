# PRX cooperativo 0.2: alcance comprobado

Se implementó `goldhen/plugin/bridge.cpp` con una ABI C versionada (`bridge.h`) y se generó un
PRX real dentro del nuevo PKG. No se instalan detours, parches ni capturas sobre procesos ajenos.
La carga del módulo PS4 todavía no se ha probado en una consola.

## Funciones conectadas

| Función | Conexión real | Evidencia / límite |
|---|---|---|
| L3+R3 y navegación | `pad` actualiza el mismo `Menu` cuya configuración usa el motor | Prueba host de ABI; aplicación nativa pasa botones de `scePadReadState`; mando físico pendiente |
| Captura | `capture` copia filas RGBA8 desde una superficie proporcionada por la aplicación propia | Rechaza pitch/capacidad/formato/banderas incompatibles; prueba host con imágenes reales en memoria |
| Interpolación | Captura dos imágenes y ejecuta el Processor optimizado, con configuración del menú | La prueba entrega un intermedio diferente de ambos originales; los multiplicadores pertenecen al motor probado |
| Overlay | Se rasteriza después de interpolar, sobre buffers propios | Nunca se utiliza el overlay como entrada de estimación; no está conectado a juegos |
| Presentación | Callback con dimensiones, pitch, posición temporal y pixels reales | En la app llama a VideoOut y confirma tras completar el flip; en el test consume fuera de pantalla y confirma cero flips |
| Preferencias | La app copia la configuración del módulo y la guarda por su identificador | Motor OFF en cada proceso; migración por lectura del archivo anterior, sin borrarlo |
| Telemetría | Se separan capturas, imágenes entregadas y flips confirmados | Entregar a un archivo no incrementa los flips |
| Captura/presentación en juegos | No conectada | `game_hooks=0` en la tabla de capacidades; no aparece como opción activable |
| GPU | No implementada | `gpu_backend=0`; no existe un botón GPU decorativo |

Los punteros del callback solo son válidos hasta su retorno. El llamante debe conservar la entrada
durante `capture` y garantizar que la CPU puede leerla; una bandera no sustituye un fence de GPU.
Solo se admite RGBA8 lineal tratado como opaco. No se admite memoria tiled, HDR ni formatos arbitrarios.
La aplicación es la dueña de sus buffers y puede satisfacer ese contrato. Un juego ajeno requiere
un adaptador específico que demuestre propiedad, sincronización y vida útil de sus superficies.

La reentrada desde un callback devuelve `FG_BUSY` mediante una bandera atómica; no bloquea sobre
un mutex recursivo. Los errores desactivan el motor. Antes de destruir un contexto o descargar el
módulo hay que dejar de emitir llamadas; la aplicación propia destruye sus contextos antes del unload.
El retorno de rechazo durante unload por GoldHEN deberá comprobarse en consola antes de depender de él.

## Probar en la aplicación propia

El PRX está empaquetado, pero **no se carga automáticamente**. La aplicación usa por defecto el
motor integrado. Una vez confirmada la compatibilidad e instalada la nueva aplicación:

1. Comprobar primero el modo integrado y sus logs.
2. Crear manualmente el marcador `/data/PS4FrameGen/enable_cooperative_prx` para solicitar la carga
   en el siguiente arranque. No se modifica la configuración global de plugins de GoldHEN.
3. La app carga `/app0/framegen_bridge.prx`, resuelve `fg_bridge_api`, comprueba ABI/capacidades y
   pasa sus imágenes y su callback VideoOut. El log debe decir `COOPERATIVE PRX CONNECTED`.
4. L3+R3 abre el menú del módulo. Sus controles están conectados al motor del módulo; se guarda
   su configuración desde la app. El motor comienza desactivado.
5. Si carga o ABI falla, se registra el fallo y continúa el motor integrado. Un fallo durante la
   captura/presentación termina la app con diagnóstico; no se oculta tras contadores.
6. Para volver al modo integrado sin abrir un juego, renombrar el marcador con la aplicación cerrada.
   Conservar el archivo si se desea reactivar la prueba. No hay que borrar archivos del usuario.

Esto es un modo experimental compilado y conectado en el código nativo. La prueba host ejecuta
la misma implementación como biblioteca compartida Linux; no demuestra carga del PRX en Orbis.

## Investigación del repositorio oficial de plugins

Se inspeccionó [GoldHEN_Plugins_Repository](https://github.com/GoldHEN/GoldHEN_Plugins_Repository),
commit `998fe03fe0275b59310511187ba8550ed475a003`:

- `gamepad_helper` demuestra hooks documentados sobre `scePadReadState`.
- `fliprate_remover` y `force_30_fps` interceptan `sceVideoOutSetFlipRate`; cambiar ese límite no genera imágenes.
- `force_1080p_display` cambia los números devueltos por `sceVideoOutGetResolutionStatus`; su propio comentario
  dice que **reporta** una pantalla 1080p. No demuestra escalado real y no se usa como escalador.
- La plantilla exporta `plugin_load`, `plugin_unload` y metadatos. El PRX nuevo exporta esas entradas
  y su API C; el verificador comprueba que figuren en la tabla dinámica.

No se copiaron ni activaron los parches de esos plugins. Un hook de mando o de flip es una pieza
posible de integración, pero no resuelve acceso a la imagen, tiling, fences, buffers extra y cadencia.
El siguiente paso en juegos requiere un título controlado y permiso/contrato de sus superficies,
seguido de pruebas físicas; no existe una ruta universal implementada.
