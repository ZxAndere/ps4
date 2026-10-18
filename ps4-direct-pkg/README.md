# PS4 Direct PKG v1.1 — Descarga + instalacion experimental

Este proyecto compila un PKG nativo para PS4 que funciona sin NovaPS ni PC encendido. Es una prueba para paquetes **homebrew autorizados**, no un gestor de enlaces a juegos sin licencia.

## Funcionamiento

1. Mantener `/data/ps4direct_url.txt` con una URL HTTP(S) de un PKG permitido.
2. Iniciar v1.1; descargar a `/data/*.part` o reanudar si hay una descarga parcial compatible.
3. Al terminar se guarda como `/data/*.pkg` y se pide confirmacion adicional.
4. Si aceptas, intenta registrar una tarea de instalacion local mediante **BGFT**, con ruta del sistema `/user/data/*.pkg`.
5. Muestra el ID de tarea y los errores. Que la tarea haya comenzado **no significa** que la instalacion haya terminado; verificar en Notificaciones de PS4.

Si el archivo ya estaba descargado, se puede reabrir v1.1 para solicitar su instalacion, **sin volver a descargarlo**.

## Medidas

- Nunca desinstala versiones existentes, ni reemplaza el propio instalador.
- No borra el PKG original.
- No intenta instalar archivos `.part`.
- Verifica cabecera, tamanos, coincidencia de URL con el registro local; tambien verifica encabezado y titulo usando APIs nativas.
- Requiere confirmacion explicita para descargar e instalar.
- Bloquea instalacion si un titulo ya esta instalado.
- Necesita **espacio suficiente tanto para el PKG almacenado como para que el sistema lo instale**. No comprueba automaticamente espacio libre.
- Sin comprobacion criptografica del paquete completo. Usa solamente fuentes homebrew confiables.
- No se ha probado v1.1 en hardware; build correcto no asegura que la tarea BGFT funcione.

## Compilacion

Rama `ps4-direct-pkg-experimental` en ZxAndere/ps4, GitHub Actions, OpenOrbis v0.5.4. Identificador independiente `BREW00978`; no modifica v1.0 ni versiones previas.
