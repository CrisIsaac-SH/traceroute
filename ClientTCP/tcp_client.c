/*
 * tcp_client.c
 *
 * Entry point for the raw TCP client.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tcp_client.h"
#include "tcp_session.h"

static const option_t options[] = {
    {"-l", "--local", OPT_LOCAL},
    {"-s", "--server", OPT_SERVER},
    {"-p", "--port", OPT_PORT},
    {"-c", "--client-port", OPT_CLIENT_PORT},
    {"-m", "--message", OPT_MESSAGE},
    {"-i", "--interface", OPT_INTERFACE},
};

void print_usage(const char *prog, const cli_options_t *o)
{
    printf("\nUso: %s [opciones]\n\n", prog);
    printf("  -l, --local IP          IP local (default: %s)\n", o->local_ip);
    printf("  -s, --server IP         IP del servidor (default: %s)\n", o->server_ip);
    printf("  -p, --port PORT         Puerto del servidor (default: %u)\n", o->server_port);
    printf("  -c, --client-port PORT  Puerto local cliente (default: aleatorio 40000-49999)\n");
    printf("  -m, --message MSG       Mensaje inicial opcional (si no se especifica, se entra en modo interactivo)\n");
    printf("  -i, --interface IF      Interfaz de red (default: %s)\n", o->interface ? o->interface : "no especificada");
    printf("                          Linux: lo, eth0 | macOS: lo0, en0\n");
    printf("  -h, --help              Muestra esta ayuda\n\n");
}

const option_t *find_option(const char *arg)
{
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++)
        if (!strcmp(arg, options[i].short_name) || !strcmp(arg, options[i].long_name))
            return &options[i];

    return NULL;
}

int parse_cli(int argc, char **argv, cli_options_t *o)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            print_usage(argv[0], o);
            return 1;
        }

        const option_t *option = find_option(arg);
        if (!option) {
            fprintf(stderr, "Opción desconocida: %s\n", arg);
            return -1;
        }

        if (++i >= argc) {
            fprintf(stderr, "Falta el valor para: %s\n", arg);
            return -1;
        }

        const char *value = argv[i];

        switch (option->id) {
        case OPT_LOCAL: o->local_ip = value; break;
        case OPT_SERVER: o->server_ip = value; break;
        case OPT_PORT: o->server_port = (uint16_t)atoi(value); break;
        case OPT_CLIENT_PORT: o->client_port = (uint16_t)atoi(value); break;
        case OPT_MESSAGE: o->message = value; break;
        case OPT_INTERFACE: o->interface = value; break;
        }
    }

    return 0;
}

int run_session(const cli_options_t *o)
{
    tcp_session_t session;

    if (tcp_session_open(&session, o->local_ip, o->server_ip, o->server_port, o->client_port, o->interface) != 0)
        return 1;

    if (tcp_session_handshake(&session, 10) != 0) {
        tcp_session_abort(&session);
        return 1;
    }

    printf("──────────────────── DATA ──────────────────────────\n");

    /* Si se proporcionó un mensaje por argumento CLI, enviarlo */
    if (o->message != NULL && strlen(o->message) > 0) {
        int rc = tcp_session_send_message(&session, o->message);
        if (rc < 0) {
            tcp_session_abort(&session);
            return 1;
        }
        if (rc == 1) {
            /* Conexión cerrada normalmente por mensaje de terminación (ej. EXIT OFF) */
            return 0;
        }
    }

    /* Modo interactivo: permite enviar N mensajes personalizados */
    char input_buf[TCP_SESSION_MAX_MSG];
    while (1) {
        printf("\nIngrese un mensaje: ");
        fflush(stdout);

        if (!fgets(input_buf, sizeof(input_buf), stdin)) {
            printf("\nFin de entrada detectado. Enviando 'EXIT OFF' para cerrar sesión...\n");
            tcp_session_send_message(&session, "EXIT OFF");
            break;
        }

        /* Remover saltos de línea al final */
        size_t l = strlen(input_buf);
        while (l > 0 && (input_buf[l - 1] == '\n' || input_buf[l - 1] == '\r')) {
            input_buf[--l] = '\0';
        }

        if (l == 0) continue;

        int rc = tcp_session_send_message(&session, input_buf);
        if (rc != 0) {
            /* 1 = cerrada por el servidor y finalizada por el cliente; <0 = error */
            break;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    cli_options_t opts = {
        .local_ip = "127.0.0.1",
        .server_ip = "127.0.0.1",
        .server_port = 5001,
        .client_port = 0,
        .message = NULL,
        .interface = NULL,
    };

    int rc = parse_cli(argc, argv, &opts);
    if (rc != 0) return rc > 0 ? 0 : 1;

    srand((unsigned)time(NULL));
    return run_session(&opts);
}