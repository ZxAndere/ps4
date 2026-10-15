# BOOT05 — prueba controlada de arranque

**No está validado en consola.** BOOT01 falló físicamente; no se debe continuar
con BOOT02–04. PS4 Cheat Manager sí abre en la sesión del usuario.

**[Descargar PKG BOOT05](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot05/IV0000-FGBT00005_00-BOOTDIAG00000005.pkg)**

Archivo: `IV0000-FGBT00005_00-BOOTDIAG00000005.pkg`.
Tamaño: **6.619.136 bytes** (6,31 MiB).
SHA-256:

```text
b50a67d5400eca1560531194cab50b0457440cfabd95c43ce41f2ea26281a527
```

- [Código fuente e informes ZIP](https://github.com/ZxAndere/ps4/raw/refs/heads/main/downloads/boot05/PS4-FrameGen-BOOT05-source.zip).
- [SHA256SUMS](SHA256SUMS) y [manifiesto](manifest.json).
- [Informe técnico y diferencias comprobadas](../../PS4-FrameGen/docs/boot05/DIAGNOSTICO.md).
- [Prueba segura y recuperación](../../PS4-FrameGen/docs/boot05/PRUEBA_PS4.md).
- [Registro de cambios](../../PS4-FrameGen/docs/boot05/CAMBIOS.md).
- [Constructor revisado y reproducción](../../PS4-FrameGen/boot05/README.md).

## Qué se está probando

OpenOrbis prescribe dos stubs de compatibilidad en `sce_module/`: `libc.prx` y
`libSceFios2.prx`. Faltan en BOOT01 y están presentes en el ejemplo oficial y en
la receta pública de PS4 Cheats Manager. BOOT05 añade los del SDK oficial verificado
y conserva **exactamente el ejecutable y el icono de BOOT01**. No incluye el puente
de FrameGen ni activa generación, vídeo, mando o plugins de GoldHEN.

La causa del aviso sigue sin confirmarse: el eboot no importa directamente esos
dos archivos. Esta prueba comprueba una hipótesis documentada, sin prometer solución
ni compatibilidad con firmware 13.52 / GoldHEN «beta 12».

## Resultado esperado

Abre **FrameGen BOOT05 MODULE CONTROL**, ID `FGBT00005`. Su icono conserva el 1.
Si llega al programa, mostrará **«FrameGen BOOT01: MAIN alcanzado»** y saldrá tras
unos 20 segundos. El texto BOOT01 es intencional porque se mantiene su ejecutable
sin cambios. No aparece una interfaz gráfica.

Si vuelve el aviso de datos dañados, detén la prueba. No continúes con BOOT02–04.
Antes de instalar, verifica también el hash de la copia USB si usas ese medio.

Los hashes/firmas fake del PKG, extracción y comprobaciones diferenciales pasan,
además de 12 pruebas locales. Esto no prueba arranque físico. El ZIP incluye fuentes,
informes y el PKG BOOT01 como entrada de reproducción; el SDK y los archivos históricos
completos siguen fuera de ese ZIP. Las versiones previas permanecen intactas.
