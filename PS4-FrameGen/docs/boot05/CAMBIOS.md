# Cambios de BOOT05

15 de octubre de 2026 (UTC−4).

- Registrado el fallo físico de BOOT01 y el dato nuevo: PS4 Cheat Manager abre
  en la misma sesión. No se continúa con BOOT02–04 ni con funciones de FrameGen.
- Conservados los 256 hashes de la instantánea previa. Se mantienen sin cambios
  255 archivos anteriores; solo se actualiza la portada del repositorio con el
  estado actual. Se guarda una copia de esa portada en la instantánea anterior.
- Compilado el `hello_world` oficial en una carpeta independiente con su Makefile.
  Comparados CRT, ELF/SELF, importaciones, linker, SFO, PKG, módulos y authinfo con
  BOOT01 y las fuentes actuales de PS4 Cheats Manager.
- Identificada la omisión de dos stubs que prescribe OpenOrbis para los paquetes.
  Se registra como hipótesis de arranque, sin atribuirle causalidad física.
- Reproducidos los objetos y ELF de BOOT01. La conversión incorpora la ruta de
  entrada en `DT_SCE_FILENAME`; el primer constructor detuvo la prueba al detectar
  un SELF diferente en otra carpeta. Esa salida permanece en `build-boot05-02`.
- BOOT05 se construye en `build-boot05-03`: mismo SELF e icono del BOOT01 publicado;
  se añaden `sce_module/libc.prx` y `sce_module/libSceFios2.prx` del SDK verificado.
  Solo cambian TITLE, TITLE_ID y CONTENT_ID, más los metadatos derivados del PKG.
- No se añade `right.sprx`, documentado para créditos en Información; no se copian
  la categoría, atributos o permisos extra de PS4 Cheats Manager.
- Añadido auditor SELF con cabeceras, segmentos y tablas SCE, verificación del
  contenido del PKG y 12 pruebas con casos negativos y preservación histórica.
- Publicados código, evidencias, PKG, SHA-256, prueba segura y recuperación.

Pendiente: arrancar físicamente BOOT05. No hay versión completa de FrameGen nueva
ni afirmación de corrección, compatibilidad de firmware o integración en juegos.
