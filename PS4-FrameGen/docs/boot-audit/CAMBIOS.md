# Registro de cambios — boot-audit-01

15 de octubre de 2026 (UTC−4).

- Preservados los originales v0.1/v0.2/v0.3. Añadidos inventario SHA-256 y registro
  del fallo físico comunicado por el usuario; no se sustituyen versiones anteriores.
- Auditado el PKG v0.3 publicado: integridad, extracción, metadatos reales,
  identidades, rutas, ELF/OELF/fake SELF, entrada, memoria, importaciones y enlace.
- Detectada cobertura BSS ocho bytes corta después de convertir v0.3 y diferencia
  de alineación TLS. Añadido script de enlace local que las elimina; SDK intacto.
- Comprobado el enlace nuevo con los mismos objetos originales de v0.3. Se publica
  evidencia estática, sin declarar que resuelva el aviso físico de la consola.
- Creados cuatro PKG de diagnóstico independientes: entrada C mínima, gráficos,
  imagen/texto/C++/mando y pool CPU con paso del original. FG siempre apagado;
  ningún paquete contiene o carga un PRX.
- Añadidos logs opcionales por etapa, comprobación del primer flip, espera limitada
  y cierre de vídeo antes de liberar sus buffers. OPTIONS solo en BOOT03/04.
- Añadidos inspector Orbis, lector SFO y 12 regresiones con casos negativos.
  Los cuatro paquetes superan validación y extracción local.
- Documentados límites de la investigación GoldHEN, prueba física y recuperación.
  No se declara compatibilidad con 13.52 ni con «beta 12».

Iteraciones locales conservadas: `build-boot-audit-01` paró al buscar el SFO como
archivo del PFS; el SFO pertenece a las entradas de metadatos. Se corrigió la
verificación sin cambiar los criterios de integridad. `build-boot-audit-02`
completó los cuatro PKG que se distribuyen. Una conversión exploratoria también
paró por no tener activado `OO_PS4_TOOLCHAIN`; la reproducción final usa el entorno
documentado y se completó. Se conservan los logs de esas iteraciones.

Pendiente: resultados físicos de BOOT01–04; corrección adicional según la primera
etapa que falle; solo después, paquete completo de FrameGen y nueva validación
física. No se ha trabajado en funciones nuevas de generación dentro de juegos.
