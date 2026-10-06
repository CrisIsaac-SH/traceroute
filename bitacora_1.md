# Bitacora 1: Requisitos y diagnostico

## Objetivo
Leer `Proyecto_-_Traceroute.pdf` y comparar sus entregables con el contenido inicial del repositorio.

## Hallazgos
- El PDF exige una implementacion propia de traceroute en C, Python y Perl.
- Deben utilizarse RAW sockets y construirse manualmente los datagramas necesarios.
- Los parametros requeridos son TTL inicial, cantidad maxima de saltos, probes por salto, timeout y pausa entre probes.
- La salida debe mostrar salto, nombre DNS cuando exista, IP, tres tiempos y `*` para timeouts.
- El repositorio inicial contiene un laboratorio de cliente TCP raw, pero no contiene traceroute ni las pruebas comparativas solicitadas.

## Fases definidas
1. Implementar probes UDP/IPv4 manuales y recepcion de ICMP en C, Python y Perl.
2. Documentar compilacion, ejecucion, permisos, teoria y diferencias frente al traceroute del sistema.
3. Agregar pruebas reproducibles de parametros y validaciones estaticas.
4. Registrar cada objetivo o cambio posterior en otra bitacora numerada.
