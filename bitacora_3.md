# Bitacora 3: Documentacion y verificacion

## Objetivo
Documentar las fases del proyecto, enlazar el nuevo entregable desde el README principal y verificar la sintaxis disponible en el entorno.

## Cambios
- Se agrego `Traceroute/README.md` con fases, algoritmo, parametros, ejecucion, pruebas, comparacion y limitaciones.
- Se agrego `Traceroute/tests.sh` para compilar C y validar Python/Perl en Linux.
- Se agrego una referencia al proyecto de traceroute en `README.md`, manteniendo separado el laboratorio TCP existente.
- Se corrigio el manejo de `getaddrinfo` y `getnameinfo` en la implementacion Perl para usar sus valores de retorno de forma portable.

## Verificacion
- `python3 -m py_compile traceroute.py`: correcto.
- `gcc` no esta instalado en este entorno Windows, por lo que la compilacion C debe ejecutarse en Linux.
- `perl` no esta instalado en este entorno Windows, por lo que la validacion Perl debe ejecutarse en Linux.
- No se ejecuto una prueba de red raw porque este entorno no ofrece el toolchain POSIX ni privilegios raw socket.
