# Guia de compilacion y ejecucion en Kali Linux

Esta guia prepara una VM con Kali Linux para compilar y probar las implementaciones de traceroute de este proyecto. Los programas construyen probes IPv4/UDP y reciben respuestas ICMP mediante raw sockets. Para ejecutarlos se requieren permisos de `root` o la capacidad `CAP_NET_RAW`.

## 1. Preparar la maquina virtual

1. Instala Kali Linux en VirtualBox, VMware u otro hipervisor.
2. Configura el adaptador de red de la VM:
   - **NAT** es una opcion sencilla para tener salida a Internet.
   - **Adaptador puente (Bridged)** conecta la VM directamente a la red local; puede estar restringido por la red o el hipervisor.
3. Inicia Kali y comprueba que tiene conectividad:

```bash
ip address
ip route
ping -c 3 8.8.8.8
```

Si `ping` no funciona, verifica el adaptador de red de la VM, la puerta de enlace y las reglas de firewall antes de continuar. Algunas redes bloquean ICMP; eso no necesariamente significa que UDP o DNS tampoco funcionen.

## 2. Instalar las herramientas necesarias

Abre una terminal en Kali y ejecuta:

```bash
sudo apt update
sudo apt install -y build-essential python3 perl traceroute
```

Esto instala `gcc`/`make`, Python 3, Perl y el comando `traceroute` del sistema que se utilizará para comparar resultados.

## 3. Copiar el proyecto a Kali

Transfiere la carpeta `Traceroute` desde el equipo anfitrión a la VM, por ejemplo mediante una carpeta compartida, SCP o una unidad USB. En los siguientes ejemplos se supone que está en `~/Traceroute`. Ajusta la ruta si la copiaste a otra ubicación.

```bash
cd ~/Traceroute
ls
```

Debes ver al menos `Makefile`, `traceroute.c`, `traceroute.py`, `traceroute.pl` y `tests.sh`.

## 4. Compilar la implementación en C

Desde la carpeta del proyecto:

```bash
make clean
make
```

La compilación debe crear el ejecutable `traceroute_c`. Opcionalmente, comprueba sus opciones:

```bash
./traceroute_c --help
```

Si `make` o `gcc` no se encuentran, instala las herramientas de compilación del paso 2. Si aparecen errores del compilador, guarda el mensaje completo para corregirlos antes de hacer la prueba de red.

## 5. Ejecutar las validaciones estáticas

El script incluido valida la compilación de C y la sintaxis de Python y Perl:

```bash
bash tests.sh
```

El resultado esperado al final es `Static checks passed`. Este resultado no reemplaza la prueba de red: solo confirma compilación y sintaxis.

## 6. Probar los tres programas

Ejecuta las pruebas con `sudo`, porque los programas abren raw sockets. Se recomienda empezar con pocos saltos para limitar el tiempo de espera:

```bash
sudo ./traceroute_c -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8
sudo python3 traceroute.py -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8
sudo perl traceroute.pl -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8
```

Para probar resolución de nombres en vez de una IP literal:

```bash
sudo ./traceroute_c -m 5 example.com
sudo python3 traceroute.py -m 5 example.com
sudo perl traceroute.pl -m 5 example.com
```

Cada línea corresponde a un salto. `*` significa que el probe expiró sin recibir una respuesta ICMP que coincidiera. Esto puede deberse a filtros, pérdida de paquetes o a la configuración de la red y no implica automáticamente que el programa haya fallado.

## 7. Comparar con el traceroute del sistema

Usa el mismo destino y un número similar de saltos. La implementación del proyecto usa probes UDP:

```bash
sudo traceroute -n -f 1 -m 3 -q 3 -w 1 8.8.8.8
```

Compara principalmente el orden y patrón de saltos. Las rutas y los tiempos pueden diferir entre ejecuciones por balanceo de carga, congestión, VPN, NAT, firewalls o políticas ICMP. El traceroute del sistema puede tener diferencias de implementación; no se espera necesariamente que cada tiempo coincida exactamente.

## 8. Guardar evidencia de las pruebas

Guarda la salida de cada programa en archivos separados para incluirla en la entrega:

```bash
sudo ./traceroute_c -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8 2>&1 | tee resultado_c.txt
sudo python3 traceroute.py -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8 2>&1 | tee resultado_python.txt
sudo perl traceroute.pl -f 1 -m 3 -q 3 -w 1000 -i 100 8.8.8.8 2>&1 | tee resultado_perl.txt
sudo traceroute -n -f 1 -m 3 -q 3 -w 1 8.8.8.8 2>&1 | tee resultado_sistema.txt
```

Incluye las salidas o capturas, explica las diferencias relevantes y registra la configuración de red de la VM. No publiques información privada que pueda aparecer en direcciones o nombres de la red local.

## 9. Solución de problemas

### `Operation not permitted` o error al crear raw sockets

Ejecuta el programa con `sudo`. Confirma que el usuario tiene permisos administrativos y que el kernel no está restringiendo raw sockets.

### Solo aparecen asteriscos (`*`)

Comprueba conectividad y DNS:

```bash
ip route
getent hosts example.com
ping -c 3 8.8.8.8
```

Prueba otro destino y revisa el modo de red del hipervisor. NAT/firewall o la red externa pueden filtrar ICMP; repite con otro destino o desde otra red. Los asteriscos son una salida posible si los routers no responden a ICMP.

### `Temporary failure resolving` o no resuelve el destino

Verifica la conexión de la VM y su configuración DNS. Comprueba primero con `getent hosts example.com`.

### El comando del sistema `traceroute` no existe

Instálalo con:

```bash
sudo apt install traceroute
```

## Criterio de finalización

La prueba queda cerrada cuando:

- `make` compila `traceroute_c` sin errores.
- `bash tests.sh` termina con `Static checks passed`.
- Los programas C, Python y Perl se ejecutan en Kali con permisos RAW.
- Se ejecuta el traceroute del sistema para el mismo destino.
- Se guardan las salidas/capturas y se documentan las diferencias observadas.
