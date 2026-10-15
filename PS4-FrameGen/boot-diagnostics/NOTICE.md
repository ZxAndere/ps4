# Fuentes y licencia

`link-boot.x` deriva de `link.x` de
[OpenOrbis PS4-Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain),
revisión `6929bafac0b565692ed61e66032a79185ee9cf65`.
Se conserva su licencia GNU GPL versión 3 o posterior, incluida en
`LICENSE-OpenOrbis.txt`.

Cambios locales del 15 de octubre de 2026: entrada explícita `_start`, recogida
de subsecciones de datos, excepciones y TLS; conservación de inicializadores;
alineación TLS a 32 bytes y relleno previo a BSS para la conversión Orbis.
El SDK instalado no se modifica.

El código nuevo de este directorio se distribuye también bajo GPL-3.0-or-later.
Las fuentes históricas de FrameGen y los componentes del toolchain conservan
sus avisos y licencias. Véase `../docs/THIRD_PARTY.md`. No se incluyen módulos
propietarios de la consola. Los iconos se generan desde código propio.
