# Fuentes y herramientas externas

El proyecto utiliza las declaraciones de API y bibliotecas de la distribución oficial de
[OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) para compilar la aplicación PS4.
Consultar licencias de cada componente en esa distribución (toolchain, musl, libc++, LLVM y empaquetador).
No se copió un SDK propietario de Sony ni se incluyeron sus bibliotecas en el código fuente del proyecto.

[GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK), MIT, se consultó para la auditoría;
no se enlaza ni se redistribuye en el paquete actual.

El vídeo de prueba se obtiene de [opencv_extra](https://github.com/opencv/opencv_extra), ruta
`testdata/cv/video/1920x1080.avi`, commit fijado en `fetch-test-video.sh`. Consultar las condiciones
del repositorio y los datos para redistribuir el material audiovisual. No se incluye el vídeo fuente
en el ZIP del código; se proporciona la descarga verificable. Los resultados locales y capturas
derivadas se conservan para la inspección de pruebas, con su procedencia registrada.

CMake, Ninja, FFmpeg, Clang/LLD, GCC, los paquetes Debian y PkgTool conservan sus licencias propias.
