# Auditoría de viabilidad

Consulta realizada el 15 de octubre de 2026. La máquina disponible es Linux, no el Windows del usuario.

| Componente | Evidencia | Alcance |
|---|---|---|
| Host | Debian 13 x86-64; GCC 14.2; cinco CPU visibles; unos 29 GiB libres al iniciar | Motor y pruebas ejecutables aquí |
| OpenOrbis | [Release oficial v0.5.4](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4), `toolchain-llvm-18.tar.gz` | Headers, musl, libc++, stubs y empaquetadores instalados |
| LLVM | Clang/LLD 19.1.7, paquetes Debian firmados | Compilación real ELF y PKG; no implica ejecución |
| GPU | No hay dispositivo `/dev/dri` en la máquina | Sin benchmarks de GCN ni backend GPU probado |
| GoldHEN | Usuario: «beta 12», firmware 13.52 | Versión completa y soporte de ese firmware no establecidos |
| SDK GoldHEN | [Repositorio oficial](https://github.com/GoldHEN/GoldHEN_Plugins_SDK), commit `bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7` | Detours, información de proceso y carga PRX; SDK API 1.00 |
| Windows | Script de auditoría proporcionado, sin acceso a esa máquina | Compilación Windows aún pendiente |

La ruta `releases/latest` de GoldHEN redirigió a la estable 2.3; el índice oficial también expone
[2.4b18 como prerelease](https://github.com/GoldHEN/GoldHEN/releases/tag/2.4b18).
Sus notas enumeran 5.05, 6.71/6.72, 9.00, 9.60, 10.00/10.01, 10.50, 10.70/10.71 y 11.00.
Esa lista no acredita soporte de **13.52** y «beta 12» no permite identificar inequívocamente un
artefacto publicado. No se deduce que la consola no funcione: se mantiene el dato del usuario
separado de la compatibilidad que este proyecto puede verificar.

El [README de OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) exige Clang/LLD y una
distribución completa: clonar solo fuentes no instala sus bibliotecas. El SDK no suministra acceso
remoto a una PS4 ni convierte un binario compilado en una prueba de compatibilidad con un firmware.
Las consultas Git a repositorios oficiales funcionaron con la autenticación HTTPS existente.
GitHub API y el mirror `snapshot.debian.org` devolvieron 403 en la política actual; se usaron páginas
oficiales de GitHub y `deb.debian.org` con verificación de firmas APT intacta. No se solicitaron tokens.

## Presentación, buffers y refresco

Fuentes de API: [`VideoOut.h`](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/VideoOut.h),
[`_types/video.h`](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/include/orbis/_types/video.h),
y [ejemplo graphics](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/tree/v0.5.4/samples/graphics).
El ejemplo documenta presentación de buffers dibujados por CPU. Están declarados registro de buffers,
atributos de formato/tiling/pitch, flip con vsync, estado de flips y consulta del modo de pantalla.
Estos son los contratos usados por la aplicación propia. La aceptación efectiva de ambas superficies,
el orden de cierre/reapertura y su comportamiento bajo fallos requieren las pruebas en consola.

Una pantalla de 60 Hz no muestra 90 o 120 imágenes únicas/s: ×3/×4 son posiciones de interpolación.
Con una fuente real de 30 Hz, ×2 exigiría completar el trabajo dentro de 33.3 ms y poder presentar
cada imagen a tiempo; la CPU de referencia excede ese presupuesto aquí. Reducir solo la salida
no elimina el coste de interpolar primero a resolución nativa. Reducir el procesamiento sí reduce
ese trabajo y puede perder detalle. Los resultados cuantifican ambos casos sin extrapolarlos a PS4.

## Integración GoldHEN: hechos e hipótesis

**Documentado:** `GoldHEN.h` declara `sys_sdk_proc_info`, que incluye título, y operaciones PRX;
`Detour.h` ofrece herramientas de hooks. El SDK depende de OpenOrbis y LLVM 10+.
**No implementado ni demostrado aquí:** interceptar un juego, obtener sus superficies o insertar flips.

Un detour en presentación solo observa una llamada. No establece disponibilidad de la imagen,
propiedad de la memoria, formato lineal, finalización del trabajo GPU, fences, vida de los buffers,
presupuesto de cómputo ni permiso de insertar nuevos buffers en la cola. Muchos símbolos gráficos
del SDK están sin prototipo completo. Se necesita ingeniería por backend/título y validación física.
La resolución interna depende de render targets, shaders, HUD y lógica del juego; no hay en el SDK
citado una operación universal documentada que la reconfigure con seguridad.

**Hipótesis a investigar:** subir el estimador y los warps a compute shaders GCN en la aplicación
controlada, o usar un backend gráfico libre cuya creación/sincronización de superficies esté probada.
Ni el rendimiento esperado ni la compatibilidad con juegos se consideran resultados.
La falta de ese contrato no prueba imposibilidad matemática universal; sí impide prometerla con
la evidencia y el SDK disponibles. Por eso se entrega una aplicación independiente, sin hooks activos.

## Huellas de herramientas y datos

- SDK OpenOrbis: SHA-256 `3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526`.
- Vídeo OpenCV: commit `42a04e79550c8a58a0ec1b41aa2bce25236dbb0e`, blob Git `528c4c9a6484e02e6726737307e4f9f576167e1b`.
- Vídeo SHA-256: `42f77d89c19428a06c70e3c8f455cc3cced40bd0b46dafee3bfbfde323362153`.

Las huellas fijan los bytes descargados a través de HTTPS oficial; no se presentan como firmas
independientes del editor. No se desactivaron comprobaciones TLS, firmas de paquetes ni hashes.
