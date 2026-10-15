# PS4 FrameGen — diagnóstico de arranque

**v0.3 falla al abrir en la PS4 Fat del usuario:** instala, pero muestra el aviso
de datos dañados. Todavía no hay corrección confirmada en consola.

**[Descargar BOOT01, control mínimo de arranque](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot-audit-01/IV0000-FGBT00001_00-BOOTDIAG00000001.pkg)**

- [Los cuatro diagnósticos y sus hashes](downloads/boot-audit-01/README.md).
- [Cómo probarlos y recuperación](PS4-FrameGen/docs/boot-audit/PRUEBA_PS4.md).
- [Auditoría y límites de lo comprobado](PS4-FrameGen/docs/boot-audit/DIAGNOSTICO.md).
- [Registro de cambios](PS4-FrameGen/docs/boot-audit/CAMBIOS.md).

Se corrigió un defecto estático de cobertura BSS y se construyeron cuatro pruebas
progresivas, sin PRX y con FG desactivado. Pasan integridad del PKG y 12 regresiones;
falta ejecutarlas en PS4. No se declara compatibilidad con 13.52 / GoldHEN beta 12.
Los archivos v0.1, v0.2 y v0.3 permanecen intactos.

---

# Descarga histórica v0.3

La siguiente información corresponde a la entrega original, anterior al informe
de fallo físico. Conservada como referencia; el PKG no debe considerarse validado
en consola.

**[Abrir las descargas de v0.3](downloads/v0.3/README.md)**

- [Proyecto completo ZIP](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/v0.3/PS4-FrameGen-0.3-project-final.zip)
- [Aplicación PS4 PKG](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/v0.3/IV0000-FGEN00003_00-PS4FRAMEGEN00003.pkg)
- [Plugin PRX](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/v0.3/framegen_bridge.prx)
- [Ejecutables de pruebas Windows](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/v0.3/PS4-FrameGen-0.3-windows.zip)
- [Informe de resultados](PS4-FrameGen/docs/RESULTS_V03.md) y [recuperación](PS4-FrameGen/docs/INSTALL_V03.md).

37 casos funcionales e integración cooperativa pasan en Linux, también con ASan/UBSan.
PKG y PRX compilados y verificados estructuralmente; sin ejecución física en PS4.
No se declara compatibilidad con firmware 13.52, GoldHEN «beta 12» o juegos externos.
Los EXE Windows son compilaciones reales, pendientes de ejecutar en Windows.

El ZIP contiene las versiones anteriores y los archivos de referencia que no se duplican
como árboles de build en Git. Los archivos originales de la nube se conservan.

---

# PS4 FrameGen

Código, instrucciones y resultados en [PS4-FrameGen](PS4-FrameGen/README.md).
Prototipo C++ real con interpolación, escalado y aplicación nativa compilada.
No está verificado en consola ni integrado en juegos comerciales.
