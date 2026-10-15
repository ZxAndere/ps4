# Aceleración GPU para PS4 Fat: investigación 0.2

El backend ejecutado y medido es CPU. PS4 Fat dispone de GPU AMD GCN con cómputo; esa capacidad
del hardware no implica disponer aquí de un runtime GPU utilizable ni un formato de shader válido.
No hay dispositivo `/dev/dri`, consola conectada, dirección de PS4 configurada ni emulador Orbis
instalado en este entorno. No se publican tiempos de GPU ni una opción GPU sin implementación.

La cabecera OpenOrbis `include/orbis/GnmDriver.h` declara nombres como `sceGnmDispatchDirect`,
`sceGnmMapComputeQueue` y `sceGnmDispatchInitDefaultHardwareState` con prototipos `void ...()`
incompletos. También declara funciones de envío de command buffers. Eso permite localizar APIs
para investigar, pero no proporciona por sí mismo el layout de comandos, descriptores de recursos,
compilación/empaquetado de shaders ni un protocolo completo de sincronización.

El ejemplo `samples/piglet/piglet/PigletApplication.cpp` del SDK carga `libScePigletv2VSH.sprx` y
`libScePrecompiledShaders.sprx`, y busca shaders precompilados. No demuestra que este proyecto pueda
compilar e instalar kernels arbitrarios, ni que esos módulos y privilegios estén disponibles en firmware
13.52. No se descargaron módulos propietarios ni se modificaron archivos del sistema para forzarlo.

## Ruta viable de investigación

1. Verificar una biblioteca gráfica o un backend GNM libre sobre la aplicación propia, con sus herramientas
   de shader y una PS4 de prueba. Compilar instrucciones GCN fuera de línea no basta: un objeto HSA de
   AMD para PC no es automáticamente un shader PS4 cargable.
2. Implementar primero escalado y el warp sobre imágenes propias. Usar bloques de trabajo y memoria
   compartida para reutilizar luminancia/teselas; comprobar contornos, pitch, sRGB y formato de salida.
3. Medir estimación, warps, conversión tiled/lineal, esperas y presentación sobre el hardware real. Considerar
   SIMD por ola, coste de tráfico y competencia con el renderizador, sin asumir recursos GPU libres.
4. Comparar las imágenes con el backend CPU conservado y medir todos los modos y resoluciones.
5. Solo después adaptar un título compatible que pueda proporcionar buffers y fences válidos.

SSE2 en la CPU de PS4 sí se ha integrado en los binarios compilados; se verifican instrucciones
`mulps`, `addps`, `punpcklbw` y `cvttps2dq`. No se usa `-march=native`, AVX2 ni AVX-512 para obtener
las cifras del host. El procesador EPYC del host y el Jaguar de PS4 tienen rendimiento diferente;
los tiempos del host no se extrapolan a la consola.
