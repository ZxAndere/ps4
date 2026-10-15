# PS4 FrameGen — diagnósticos de arranque

**Pendientes de prueba física en PS4.** Estos paquetes no reemplazan v0.3 ni se
presentan como compatibles con firmware 13.52 / GoldHEN «beta 12».
Fecha: 15 de octubre de 2026 (UTC−4).

## Descargar

| Orden | Descarga directa | Qué comprueba |
| --- | --- | --- |
| 1 | [BOOT01 — PKG mínimo](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/IV0000-FGBT00001_00-BOOTDIAG00000001.pkg) | Entrada a `main`: notificación, sin interfaz gráfica, sale tras 20 s |
| 2 | [BOOT02 — gráficos](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/IV0000-FGBT00002_00-BOOTDIAG00000002.pkg) | Número verde `02` en fondo azul, sale tras 30 s |
| 3 | [BOOT03 — C++, imagen y mando](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/IV0000-FGBT00003_00-BOOTDIAG00000003.pkg) | Imagen BOOT03, FG OFF, OPTIONS para salir |
| 4 | [BOOT04 — pool CPU](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/IV0000-FGBT00004_00-BOOTDIAG00000004.pkg) | Pool de dos hilos y paso del original, FG OFF |

Cada PKG ocupa 6.619.136 bytes (6,31 MiB). No contiene PRX ni necesita texturas,
fuentes o configuración externas. Sus Title ID son `FGBT00001`–`FGBT00004`.

- [Fuentes e informes ZIP](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/PS4-FrameGen-boot-audit-01-source.zip).
- [SHA256SUMS](SHA256SUMS) y [manifiesto](manifest.json).
- [Instrucciones de prueba y recuperación](../../PS4-FrameGen/docs/boot-audit/PRUEBA_PS4.md).
- [Informe de diagnóstico](../../PS4-FrameGen/docs/boot-audit/DIAGNOSTICO.md) y
  [cambios](../../PS4-FrameGen/docs/boot-audit/CAMBIOS.md).

Empieza por BOOT01. Si vuelve a aparecer el aviso de datos dañados, detén la serie
y comunica ese resultado. Una pantalla negra sin notificación no demuestra que
BOOT01 haya llegado a `main`; las instrucciones explican ese caso.

## Qué está comprobado

Los cuatro paquetes compilan para PS4 y pasan la validación de hashes y firmas
fake de las herramientas. Se extrajeron y compararon sus ejecutables y metadatos.
Pasan 12 pruebas de regresión. Eso no demuestra arranque ni compatibilidad física.

La auditoría de v0.3 encontró ocho bytes de BSS fuera del segmento declarado y una
diferencia de alineación TLS. El nuevo enlace elimina ambos problemas estáticos,
incluso al usar los objetos originales. La causa del aviso físico sigue pendiente.

El ZIP contiene fuentes e informes; no incluye el SDK, los cuatro PKG ni todos los
builds anteriores. Las [descargas originales v0.3](../v0.3/README.md) se conservan
byte a byte, junto con las versiones anteriores en su proyecto ZIP.
