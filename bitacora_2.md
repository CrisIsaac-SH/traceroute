# Bitacora 2: Implementacion del traceroute

## Objetivo
Agregar la implementacion faltante exigida por `Proyecto_-_Traceroute.pdf` sin modificar el laboratorio TCP existente.

## Cambios
- Se creo `Traceroute/traceroute.c` con probes IPv4/UDP manuales, checksum IP/UDP, TTL incremental, raw ICMP, correlacion por puertos, timeout, pausa, DNS y salida por hop.
- Se creo `Traceroute/traceroute.py` con el mismo algoritmo usando `struct`, raw sockets y `select`.
- Se creo `Traceroute/traceroute.pl` con empaquetado manual y sockets raw mediante `Socket`.
- Se agrego `Traceroute/Makefile` para compilar C.
- Se agrego `Traceroute/tests.sh` para ejecutar compilacion y validaciones de sintaxis.

## Resultado
La carpeta ya contiene las tres opciones de lenguaje solicitadas. La salida incluye los tres probes por hop y `*` cuando expira el timeout.
