# Estado del proyecto: Traceroute con RAW sockets

## Resumen general

Este proyecto corresponde a la implementación educativa de un `traceroute` manual, sin utilizar la utilidad del sistema ni librerías que lo encapsulen. La idea central es construir los paquetes IP/UDP a mano, enviar probes con TTL incremental y recibir respuestas ICMP para identificar los routers intermedios y el destino.

El repositorio original no contenía el traceroute solicitado; por eso se integró como una pieza separada del laboratorio TCP RAW existente. La carpeta actual de trabajo queda en `Traceroute/`, con el código base y documentación del proyecto.

## Fases del proyecto

### Fase 1: Análisis del requerimiento

Se revisó el PDF del proyecto y se comparó con el contenido inicial del repositorio.

Se concluyó que el proyecto exigía:
- Implementación propia de `traceroute` en C, Python y Perl.
- Uso de `RAW sockets`.
- Construcción manual de datagramas IPv4/UDP.
- TTL inicial, número máximo de saltos, probes por salto, timeout y pausa entre probes.
- Salida con hop, DNS, IP y tiempos.
- Comparación con la salida del comando del sistema.

Resultado: esta fase quedó documentada en la bitácora y en el README del proyecto.

### Fase 2: Construcción de probes

Se implementó la lógica de envío con TTL variable, incrementando el valor por salto.

Se diseñó el flujo básico:
1. Resolver la IP destino.
2. Obtener la IP local mediante una conexión UDP auxiliar.
3. Construir la cabecera IPv4 con TTL = N.
4. Agregar el encabezado UDP con un puerto destino y origen específicos.
5. Calcular checksums IP y UDP.
6. Enviar el datagrama con `SOCK_RAW`.

Esto quedó implementado en:
- `traceroute.c`
- `traceroute.py`
- `traceroute.pl`

### Fase 3: Recepción de ICMP

Se implementó la recepción de respuestas ICMP para detectar:
- `ICMP Time Exceeded` en routers intermedios.
- `ICMP Destination Unreachable / Port Unreachable` cuando llega al destino.

La validación se realiza correlacionando los puertos UDP del payload con el ICMP recibido, de forma que cada respuesta se asocia al probe correcto.

Esto es la base de la detección del siguiente salto y del tiempo de respuesta.

### Fase 4: Presentación y pruebas

La salida del programa debe mostrar:
- número del salto (`hop`)
- nombre DNS si es posible resolverlo
- dirección IP
- tres tiempos por probe
- `*` cuando el probe expira

También se documentó la necesidad de comparar la salida con `traceroute` del sistema bajo las mismas condiciones, porque la ruta puede variar según la red, firewalls, balanceo, y política ICMP.

## Qué está implementado en este momento

En la carpeta `Traceroute/` ya existen estas piezas:

- `traceroute.c` — implementación en C con raw sockets y cabeceras manuales.
- `traceroute.py` — implementación en Python con `socket`, `struct` y `select`.
- `traceroute.pl` — implementación en Perl con `Socket` y armado manual de datagramas.
- `Makefile` — compilación del programa C.
- `tests.sh` — validaciones de compilación/sintaxis.
- `README.md` — documentación central del proyecto.
- `bitacora_1.md`, `bitacora_2.md`, `bitacora_3.md` — registros de análisis, implementación y verificación.

En términos funcionales, el proyecto ya tiene la estructura base y la lógica principal del traceroute educativa implementada.

## Qué falta gestionar para tenerlo terminado

Aunque el código ya está presente, para considerar el proyecto realmente terminado aún falta la parte de validación real en entorno Linux con permisos adecuados.

### 1. Verificación real con raw sockets

Esto es obligatorio porque los raw sockets requieren:
- Linux o sistema POSIX compatible
- permisos de root o `CAP_NET_RAW`

El entorno actual es Windows y no ofrece esa infraestructura de forma nativa.

### 2. Compilar y ejecutar en Linux

Se necesita validar al menos lo siguiente:
- compilación del programa C
- ejecución correcta del script Python
- ejecución correcta del script Perl
- prueba con destinos reales como `8.8.8.8` o `example.com`

### 3. Comparación con traceroute del sistema

Debe ejecutarse la versión del sistema bajo condiciones equivalentes para comprobar:
- patrones de saltos
- tiempos estimados
- diferencias esperables por red y filtros

### 4. Evidencia para entrega

La documentación final debe incluir:
- capturas de pantalla o logs
- comparación con `traceroute` del sistema
- explicación de diferencias observadas
- video o evidencia del armado del paquete y uso de raw sockets

### 5. Ajustes finales según resultados reales

Dependiendo de la red y de los routers intermedios, puede ser necesario:
- ajustar timeouts
- revisar correlación de puertos
- manejar mejor ICMP filtrado
- corregir detalles de protocolo o checksum

## Estado actual

El proyecto está en estado de:
- implementación principal realizada
- documentación avanzada completada
- validación funcional pendiente en entorno Linux real

Por lo tanto, su estado es “casi listo técnicamente, pero aún no completamente validado y cerrado para entrega final”.

## Conclusión

La base técnica del traceroute ya está construida y documentada. Lo que falta no es reinventar el proyecto, sino lanzar las pruebas reales en Linux, registrar la evidencia y cerrar la entrega final con comparación contra el traceroute del sistema.
