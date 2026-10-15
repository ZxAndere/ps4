# Prueba física y recuperación

Estos paquetes son diagnósticos de arranque. No existe todavía confirmación de
que abran en la PS4 Fat descrita. No sustituyen v0.3: usan Title ID diferentes.

## Descargar y comprobar

1. Usa [las descargas de diagnóstico](../../../downloads/boot-audit-01/README.md).
   Empieza por **BOOT01**. No instales ni configures el PRX para esta prueba.
2. Conserva los archivos v0.1/v0.2/v0.3. Comprueba el SHA-256 del nuevo PKG antes
   de copiarlo y, si usas USB, también en el USB tras la copia. En PowerShell:

   ```powershell
   Get-FileHash -Algorithm SHA256 "IV0000-FGBT00001_00-BOOTDIAG00000001.pkg"
   ```

   Debe dar `7393781b7b26ef3818ca823084d885b7e0c3ac18efe77ef6201bc5c7d21f3d47`.
   Los otros hashes están en `SHA256SUMS` y `manifest.json` de la descarga.
3. Usa el instalador de paquetes que ya utilizaste en tu sesión habitual. No
   cambies firmware ni ajustes críticos para esta prueba. Anota la versión exacta
   que muestran los menús de consola/GoldHEN si puedes verla, sin modificarla.

## Ejecutar en orden

**BOOT01 — entrada mínima.** Abre «FrameGen BOOT01 DIAGNOSTIC». El resultado
buscado es una notificación del sistema que dice «MAIN alcanzado»; no dibuja
interfaz. Espera unos 20 segundos para su retorno. Una pantalla negra por sí sola
no demuestra ni fallo ni éxito. Si aparece de nuevo el aviso de datos dañados,
detén la serie y conserva ese resultado. No pruebes BOOT03/04 para resolverlo.

Si no aparece aviso de corrupción pero no ves notificación, la prueba es ambigua
(las notificaciones pueden estar deshabilitadas). BOOT02 ofrece una comprobación
visual; informa de esa ambigüedad al comunicar los resultados.

**BOOT02 — gráficos.** Después de BOOT01 sin error de carga, abre BOOT02.
Debe mostrar un `02` verde grande sobre fondo azul, aproximadamente 30 segundos.
La notificación «iniciando VideoOut» solo prueba que entró al programa, no que
los gráficos funcionen. La imagen o «primer flip confirmado» sí prueban que llegó
a la presentación de esta etapa. Si no aparece la imagen, detente ahí.

**BOOT03 — componentes básicos de FrameGen.** Solo después de BOOT02 visible.
Debe aparecer la imagen de diagnóstico con BOOT03, generación OFF y PRX ausente.
OPTIONS sale; sin pulsarlo, sale a los 30 segundos. No hay interruptor de FG.

**BOOT04 — pool CPU.** Solo después de BOOT03 visible. Debe mostrar BOOT04.
Construye un pool limitado a dos hilos y verifica el paso de un fotograma original
con generación desactivada. OPTIONS y la salida automática funcionan como en BOOT03.
No activa interpolación ni captura juegos.

BOOT02–04 intentan guardar `/data/PS4FrameGenBoot/BOOT02.log`, `BOOT03.log`
y `BOOT04.log`. Su escritura es opcional. Si ya tienes una forma habitual de leer
`/data`, conserva una copia de esos archivos; no hace falta habilitar servicios
nuevos para hacer la primera prueba. BOOT01 usa solo notificación y depuración
del sistema, sin crear archivos.

## Datos que permiten continuar

Comunica el último BOOT que abrió y el primero que falló, si hubo «MAIN alcanzado»,
si se vio el número/imagen y si OPTIONS o la salida automática regresaron al menú.
Incluye el texto exacto de cualquier aviso, un código CE si ahora aparece y los
logs si ya puedes acceder a ellos. Un fallo de BOOT01 apunta a carga/CRT/entorno;
que BOOT01 abra y BOOT02 falle apunta a la etapa gráfica o sus importaciones;
BOOT03/04 aíslan C++/mando y pool respectivamente. Esto delimita la investigación,
no identifica por sí solo una función culpable.

## Recuperación segura

- Ante el mismo aviso, cierra el mensaje y vuelve al menú. No repitas aperturas
  indefinidamente. Para una pantalla que siga abierta tras el tiempo previsto,
  usa el botón PS y «Cerrar aplicación».
- Si la interfaz del sistema sigue respondiendo, apaga o reinicia mediante su
  menú normal cuando lo necesites. No desconectes la alimentación mientras escriba
  en almacenamiento. Si todo el sistema deja de responder, sigue el procedimiento
  de apagado oficial de Sony para tu consola.
- Puedes quitar únicamente las aplicaciones nuevas «FrameGen BOOT01…04» desde
  el menú normal. Sus IDs son `FGBT00001`–`FGBT00004`; v0.3 es `FGEN00003`.
  Conserva logs antes de retirar los diagnósticos si quieres analizarlos.
- No hace falta borrar partidas, formatear, reconstruir la base de datos, actualizar
  firmware, tocar archivos de sistema ni modificar configuraciones de plugins.
  No se prescribe ninguna de esas acciones como solución a este aviso.

El error solo podrá considerarse corregido cuando un paquete nuevo abra físicamente
y el usuario confirme el resultado. El funcionamiento dentro de juegos reales sigue
pendiente y queda fuera de esta prueba de arranque.
