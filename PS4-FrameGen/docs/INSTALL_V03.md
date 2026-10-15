# Entrega 0.3, instalación y recuperación

El ZIP, PKG y PRX reales se guardan en `artifacts/releases/0.3`, con SHA-256 en `manifest.json`.
El PKG contiene `eboot.bin`, `framegen_bridge.prx` y `sce_sys/param.sfo`. TITLE_ID **FGEN00003**,
CONTENT_ID `IV0000-FGEN00003_00-PS4FRAMEGEN00003`, APP_VER/VERSION **01.02**. Es distinto de FGEN00001
usado en 0.2, para permitir una prueba separada. `SYSTEM_VER=0` es metadato, no prueba de soporte.
Los fake SELF requieren un entorno homebrew válido; no son binarios retail firmados por Sony.

## Verificado aquí

Compilación ELF x86-64 Orbis con OpenOrbis 0.5.4, Clang/LLD 19.1.7, SSE2, dos hilos por defecto;
exportaciones PRX; imports exclusivos Orbis; hashes/firmas comprobados por PkgTool; extracción con
payloads idénticos a los compilados; preservación de manifiestos 0.1/0.2 y entrega 0.2. Esto no prueba
instalación, arranque, comportamiento del driver ni compatibilidad de firmware. No hay PS4 conectada.

```bash
source scripts/cloud-env.sh
bash scripts/build-ps4.sh
python3 scripts/verify-ps4-artifacts.py --output artifacts/v03/pkg-verification-new
```

El directorio de compilación es `build-ps4-v03-final`; los entregables archivados son copias aparte.
No lanzar los ELF Orbis como ejecutables Linux. Los informes están en `docs/validation-v03`.

## Protocolo físico pendiente

1. Registrar modelo, versión completa y fuente de GoldHEN, firmware, pantalla y versión/hash del PKG.
   Resolver la compatibilidad del entorno antes de intentar instalar. No actualizar ni modificar firmware.
2. Instalar únicamente la aplicación propia por el procedimiento homebrew que corresponda al entorno
   verificado. Registrar un rechazo del instalador como fallo, sin reinterpretarlo como éxito.
3. Iniciar con motor OFF y PRX desactivado. Confirmar la escena, los canales de color y los valores reales
   de superficie frente al modo de pantalla. L3+R3 abre/cierra una sola vez al mantener la combinación.
4. Probar X/cruceta/Círculo y L1/R1; ×2/×3/×4; perfiles; procesamiento 1080/720/540/360 y salida 1080/720.
   Repetir 20 cambios de superficie y desactivaciones. El escalado nunca cambia un juego externo.
5. Activar JSONL y guardar al menos 60 s por modo. Contrastar flips con una capturadora si se dispone.
   Medir FPS reales, frametimes, descartes y memoria mediante una herramienta apropiada. La suma de
   capacidad RGBA no es RSS. La estimación de latencia no es una medición mando-a-fotón.
6. Comparar auto y baja latencia con normal: verificar degradación, retorno lento, originales actuales en
   sobrecarga y descarte de generados tardíos. Probar cortes bruscos y comprobar los motivos de omisión.
7. Triángulo debe desactivar inmediatamente en la siguiente lectura del mando. Options cierra la app.
   Una llamada de procesamiento o espera de flip ya en curso no se interrumpe desde el mando; el timeout
   de flip sigue siendo 3 s. No se promete una desactivación asíncrona dentro de un driver bloqueado.
8. Guardar el perfil, cerrar y reabrir: motor OFF. Probar recuperación de una configuración dañada después
   de conservar copia. Simular una salida incompleta únicamente en esta aplicación, conservando el log.
9. Solo tras comprobar el arranque propio, optar manualmente por su puente mediante el archivo local
   `/data/PS4FrameGen/enable_cooperative_prx_v03`. El marcador antiguo de 0.2 no activa 0.3. Esto carga
   `/app0/framegen_bridge.prx` en la aplicación propia; no lo añade a ningún juego ni al loader global.
10. Registrar resultados por título/build/hardware. No marcar un juego compatible a partir de pruebas host.

## Recuperación

Preferencias y registros nuevos: `/data/PS4FrameGen/v03/`. Se conservan los archivos antiguos de 0.2.
El perfil `FGEN00003.profile` mantiene `FGEN00003.profile.previous` validado antes de reemplazarlo.
«Restore previous profile» recupera el anterior con motor OFF; «Restore defaults» vuelve a los valores
iniciales sin perder el número de hilos de la instancia. Un perfil dañado recupera el anterior en modo
seguro. Un marcador `session-v03.state` RUNNING o dañado al iniciar también bloquea experimentos.
La salida normal escribe CLEAN; los fallos quedan en `faults-v03.log`.

Triángulo en ruta PRX escribe `/data/PS4FrameGen/disable_cooperative_prx_v03`, destruye la instancia
y solicita descarga. Un fallo Busy impide liberar una instancia activa. Si un driver rechaza cierre,
la memoria de presentación se retiene hasta la salida del proceso. Para volver al motor integrado,
salir de la app y conservar/desactivar el marcador de opt-in; el bloqueo tiene prioridad. Para reintentar
PRX tras revisar el fallo, conservar el bloqueo con otro nombre antes de quitarlo. No se toca firmware.
Los ajustes de modo seguro no acreditan estabilidad del sistema ni sustituyen recuperación de la consola.

## Windows

`artifacts/releases/0.3/windows` contiene EXE/DLL PE x64 reales y `test-windows-release.ps1`.
Se compilaron con MinGW 14.2; las runtimes GNU se enlazan estáticamente. La prueba del puente necesita
la DLL entregada junto al EXE. No se ejecutaron en Windows; guardar allí la salida de PowerShell al probar.

Para compilar localmente con CMake y Visual Studio C++: `powershell -File scripts/build-windows.ps1`.
El script usa `build-v03/Release` bajo Visual Studio. Para generar PE desde Linux, ejecutar
`bash scripts/setup-windows-cross.sh` tras `setup-cloud.sh`. La compilación MSVC queda pendiente;
se conservaron rutas portables y se eliminó la dependencia de `__VERSION__` para ese compilador.
El CLI acepta PPM RGB binario; `framegen_inspect ORIGINAL GENERADO REFERENCIA DIRECTORIO_NUEVO`
exporta comparación y métricas. `comparison.html` funciona sin servidor y permite elegir imágenes
reales y mover el separador. Nativa/1080/720/540/360 se comparan con referencias de tiempo conocidas.
