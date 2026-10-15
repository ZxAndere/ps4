# BOOT05: prueba única y recuperación

**BOOT01 falló físicamente. BOOT05 todavía no se ha probado en consola.**
No ejecutes BOOT02, BOOT03 o BOOT04 como parte de esta prueba.

## Preparar sin sustituir versiones

1. Descarga [BOOT05 y sus hashes](../../../downloads/boot05/README.md).
   Conserva los PKG y proyectos anteriores. BOOT05 usa `FGBT00005`; no sustituye
   BOOT01 (`FGBT00001`) ni FrameGen v0.3 (`FGEN00003`).
2. Comprueba el archivo descargado y el archivo copiado a tu USB con PowerShell:

   ```powershell
   Get-FileHash -Algorithm SHA256 "IV0000-FGBT00005_00-BOOTDIAG00000005.pkg"
   Get-FileHash -Algorithm SHA256 "E:\IV0000-FGBT00005_00-BOOTDIAG00000005.pkg"
   ```

   Cambia `E:` por tu unidad. Ambos deben dar:

   ```text
   b50a67d5400eca1560531194cab50b0457440cfabd95c43ce41f2ea26281a527
   ```

   Tamaño: 6.619.136 bytes. Si el hash no coincide, no instales esa copia.
3. Instálalo con el procedimiento habitual que ya utilizas. No cambies firmware,
   archivos del sistema, exploit ni configuración de plugins. No copies los dos
   stubs a carpetas del sistema: ya están dentro del PKG, en `sce_module/`.

## Qué observar

Abre únicamente **FrameGen BOOT05 MODULE CONTROL**. El icono conserva el número 1
porque también se mantuvo idéntico al del control original: comprueba el nombre
de la aplicación y el ID `FGBT00005` para abrir la correcta.

Si llega al programa debe aparecer una notificación del sistema:

> FrameGen BOOT01: MAIN alcanzado. Sin graficos. Salida automatica en 20 segundos.

**El texto BOOT01 es intencional en BOOT05:** utiliza exactamente el mismo
`eboot.bin` para probar solo la adición de módulos al paquete. No es una interfaz
simulada ni una confirmación previa de que funcione. No muestra imagen ni menú.
Espera aproximadamente 20 segundos para el retorno; no hay controles de mando.

- Si aparece la notificación, anota que la abriste desde la aplicación BOOT05 y
  si vuelve al menú. Eso prueba entrada a `main` de este control. Aún no prueba
  gráficos ni el arranque de FrameGen completo. No continúes con BOOT02–04.
- Si aparece el mismo aviso de datos dañados, cierra el aviso y detén las pruebas.
  La adición de módulos no habrá bastado; conserva el PKG y su hash.
- Si solo hay pantalla negra, sin notificación ni aviso, el resultado es ambiguo.
  Espera el tiempo previsto y registra si vuelve al menú. Una notificación que no
  se vea no permite decidir si llegó a `main`. No cambies ajustes críticos para
  intentar forzar una conclusión.
- Si aparece otro aviso o código CE, conserva el texto exacto. No hace falta
  ejecutar ninguna otra versión para comparar.

BOOT05 no crea logs en `/data`, no carga el puente de FrameGen, no activa FG ni
se conecta a juegos. Los dos PRX incluidos son stubs abiertos de compatibilidad
de OpenOrbis; no requieren configurar el cargador de plugins de GoldHEN.

## Recuperación segura

Si la aplicación permanece abierta después de unos 30 segundos, usa el botón PS
y «Cerrar aplicación». Ante el aviso de datos dañados, vuelve al menú y detente.
Si lo necesitas y el sistema responde, usa su apagado/reinicio normal; no cortes
la alimentación durante una escritura. Si deja de responder por completo, sigue
el procedimiento oficial de apagado de Sony para tu modelo.

No se prescribe reconstruir la base de datos, formatear, borrar partidas,
actualizar firmware ni modificar archivos críticos o configuraciones de plugins.
Puedes retirar **solo BOOT05**, desde la gestión normal de aplicaciones, buscando
el nombre y el ID `FGBT00005`. Conserva antes el PKG y el resultado de la prueba.
Las instalaciones y archivos anteriores pueden permanecer intactos.

## Resultado que necesito

Indica si abriste **BOOT05**, si viste «BOOT01: MAIN alcanzado», si regresó al menú
y si volvió el aviso de datos dañados o apareció otro mensaje. Si pudiste comprobar
el hash de la copia USB, incluye si coincidió. No se declarará corregido el fallo
hasta recibir una confirmación física de arranque.
