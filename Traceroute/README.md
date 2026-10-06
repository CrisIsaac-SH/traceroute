# Proyecto 02: Traceroute con RAW sockets

Implementacion educativa del mecanismo basico de `traceroute`. No ejecuta el comando del sistema ni usa una libreria que implemente traceroute.

## Fases del proyecto

### Fase 1: Analisis
Se identifico que el repositorio original era un laboratorio de TCP raw y no incluia el proyecto de traceroute solicitado por el PDF.

### Fase 2: Construccion de probes
Cada probe construye manualmente un header IPv4 y un datagrama UDP. El TTL cambia por salto: 1, 2, 3, etc. El checksum IP y UDP se calculan en el codigo.

### Fase 3: Recepcion ICMP
Un raw socket ICMP recibe respuestas de routers. Se aceptan `ICMP Time Exceeded` para routers intermedios y `ICMP Destination Unreachable / Port Unreachable` para el destino. La respuesta se valida revisando los puertos UDP citados dentro del mensaje ICMP.

### Fase 4: Presentacion y pruebas
Se muestran hop, nombre DNS si se puede resolver, IP y tres tiempos. Un timeout se representa con `*`. La salida debe compararse con `traceroute` del sistema bajo las mismas condiciones.

## Implementaciones

| Archivo | Uso |
|---|---|
| `traceroute.c` | C, headers IPv4/UDP construidos manualmente |
| `traceroute.py` | Python, `socket` raw y `struct` |
| `traceroute.pl` | Perl, `Socket` raw y empaquetado manual |
| `Makefile` | Compilacion del programa C |

## Parametros

Los tres programas aceptan:

| Opcion | Significado | Default |
|---|---|---:|
| `-f`, `--first-ttl` | TTL inicial | 1 |
| `-m`, `--max-hops` | cantidad maxima de saltos | 64 |
| `-q`, `--probes` | probes por salto | 3 |
| `-w`, `--timeout` | timeout por probe, en milisegundos | 3000 |
| `-i`, `--pause` | pausa entre probes, en milisegundos | 100 |

## Compilacion y ejecucion en Linux

Los raw sockets requieren `root` o `CAP_NET_RAW`. En Linux:

```bash
cd Traceroute
make
sudo ./traceroute_c -m 8 example.com
sudo python3 traceroute.py -m 8 example.com
sudo perl traceroute.pl -m 8 example.com
```

Para una prueba corta y reproducible:

```bash
sudo ./traceroute_c -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8
sudo traceroute -n -f 1 -m 3 -q 3 -w 1 8.8.8.8
```

La ruta puede cambiar entre ejecuciones por balanceo, congestion, firewall, VPN o politicas ICMP. Por eso se comparan patrones de saltos y no necesariamente tiempos exactos. El traceroute del sistema tambien puede usar ICMP o UDP con opciones distintas; esta implementacion usa UDP y recibe ICMP.

## Flujo de un probe

1. Se resuelve el destino y se obtiene la IP local mediante una conexion UDP auxiliar.
2. Se arma el header IPv4 con `TTL=N`, protocolo UDP, identificador y checksum.
3. Se arma el header UDP con un puerto destino unico para correlacionar la respuesta.
4. El router que reduce TTL a cero envia ICMP Time Exceeded.
5. Al llegar al destino, normalmente se recibe ICMP Port Unreachable porque no hay servicio en el puerto UDP elegido.
6. Se mide el tiempo desde el envio hasta la respuesta y se repite para los tres probes.

## Evidencia y video

La entrega final debe incluir capturas o logs de las dos ejecuciones, una explicacion de diferencias y un video mostrando la construccion del paquete, los raw sockets, los parametros y la comparacion. Los comandos anteriores sirven como guion de prueba; los resultados dependen de la red usada.

## Limitaciones conocidas

- Esta version implementa IPv4, UDP probes e ICMP replies.
- No implementa retransmisiones, control de congestion ni opciones de traceroute avanzadas.
- Algunos routers filtran ICMP y aparecen como `*`; esto es un resultado valido del protocolo.
- El programa debe ejecutarse en Linux o un sistema POSIX con permisos para raw sockets. El laboratorio TCP existente permanece separado.
