#include "tcp_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ip_header.h"
#include "tcp_header.h"
#include "checksum.h"
#include "raw_socket.h"

#define MAX_PACKET_SIZE 4096

static void format_tcp_flags(uint8_t flags, char *out, size_t out_size)
{
    out[0] = '[';
    out[1] = '\0';
    int first = 1;

    if (flags & TCP_SYN) {
        strncat(out, "SYN", out_size - strlen(out) - 1);
        first = 0;
    }
    if (flags & TCP_FIN) {
        if (!first) strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, "FIN", out_size - strlen(out) - 1);
        first = 0;
    }
    if (flags & TCP_RST) {
        if (!first) strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, "RST", out_size - strlen(out) - 1);
        first = 0;
    }
    if (flags & TCP_PSH) {
        if (!first) strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, "PSH", out_size - strlen(out) - 1);
        first = 0;
    }
    if (flags & TCP_ACK) {
        if (!first) strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, "ACK", out_size - strlen(out) - 1);
        first = 0;
    }
    if (flags & TCP_URG) {
        if (!first) strncat(out, ", ", out_size - strlen(out) - 1);
        strncat(out, "URG", out_size - strlen(out) - 1);
        first = 0;
    }
    strncat(out, "]", out_size - strlen(out) - 1);
}

void tcp_log_segment(const char *direction, uint8_t flags, uint32_t seq, uint32_t ack,
                     const uint8_t *payload, size_t payload_len)
{
    char flags_buf[32];
    format_tcp_flags(flags, flags_buf, sizeof(flags_buf));

    printf("%-17s %-12s SEQ=%u", direction, flags_buf, seq);

    if (flags & TCP_ACK) {
        printf(" ACK=%u", ack);
    }

    if (payload_len > 0 && payload != NULL) {
        printf(" LEN=%zu DATA=\"", payload_len);
        for (size_t i = 0; i < payload_len; i++) {
            uint8_t c = payload[i];
            if (c == '\n') printf("\\n");
            else if (c == '\r') printf("\\r");
            else if (c == '\t') printf("\\t");
            else if (c == '\\') printf("\\\\");
            else if (c == '"') printf("\\\"");
            else if (c >= 32 && c < 127) putchar(c);
            else printf("\\x%02x", c);
        }
        printf("\"");
    }
    printf("\n");
    fflush(stdout);
}

static int build_packet(uint8_t *packet, const tcp_session_t *s, uint8_t flags,
                        const char *data, size_t data_len)
{
    ip_header_t *iph = (ip_header_t *)packet;
    tcp_header_t *tcph = (tcp_header_t *)(packet + sizeof(ip_header_t));
    uint8_t *payload = packet + sizeof(ip_header_t) + sizeof(tcp_header_t);

    if (data && data_len) memcpy(payload, data, data_len);

    uint16_t tcp_len = (uint16_t)(sizeof(tcp_header_t) + data_len);

    build_tcp_header(tcph, s->local_port, s->server_port, s->seq, s->ack, flags, 5840);
    build_ip_header(iph, s->local_ip, s->server_ip, tcp_len);

    iph->checksum = calculate_checksum((uint16_t *)iph, sizeof(ip_header_t));
    tcph->checksum = calculate_tcp_checksum(iph->src_addr, iph->dst_addr, (uint8_t *)tcph, tcp_len);

    return (int)(sizeof(ip_header_t) + tcp_len);
}

static int receive_next_tcp_packet(tcp_session_t *s, tcp_header_t *out_tcph,
                                   uint8_t *out_payload, int max_payload,
                                   int *out_payload_len, int timeout_sec)
{
    static uint8_t buffer[MAX_PACKET_SIZE];
    time_t start = time(NULL);

    while (difftime(time(NULL), start) < timeout_sec) {
        int len = receive_packet(s->sockfd, buffer, sizeof(buffer), 200);
        if (len < (int)(sizeof(ip_header_t) + sizeof(tcp_header_t))) continue;

        ip_header_t *iph = (ip_header_t *)buffer;
        if (iph->protocol != IPPROTO_TCP) continue;

        int ip_len = (iph->ihl_version & 0x0F) * 4;
        if (len < ip_len + (int)sizeof(tcp_header_t)) continue;

        tcp_header_t *tcph = (tcp_header_t *)(buffer + ip_len);
        struct in_addr addr = {.s_addr = iph->src_addr};

        if (strcmp(inet_ntoa(addr), s->server_ip) != 0) continue;
        if (ntohs(tcph->src_port) != s->server_port) continue;
        if (ntohs(tcph->dst_port) != s->local_port) continue;

        if (out_tcph) memcpy(out_tcph, tcph, sizeof(*out_tcph));

        int tcp_header_len = ((tcph->data_offset_reserved >> 4) & 0x0F) * 4;
        int payload_offset = ip_len + tcp_header_len;
        int payload_len = len - payload_offset;
        if (payload_len < 0) payload_len = 0;

        if (out_payload && max_payload > 0) {
            int copy_len = payload_len < max_payload ? payload_len : max_payload;
            memcpy(out_payload, buffer + payload_offset, copy_len);
            if (out_payload_len) *out_payload_len = copy_len;
        } else {
            if (out_payload_len) *out_payload_len = payload_len;
        }

        return 0;
    }

    return -1;
}

static int send_control(tcp_session_t *s, uint8_t flags)
{
    uint8_t packet[MAX_PACKET_SIZE];
    int len = build_packet(packet, s, flags, NULL, 0);

    return send_packet(s->sockfd, packet, len, s->server_ip);
}

int tcp_session_open(tcp_session_t *s, const char *local_ip, const char *server_ip,
                     uint16_t server_port, uint16_t local_port, const char *capture_interface)
{
    memset(s, 0, sizeof(*s));

    strncpy(s->local_ip, local_ip, sizeof(s->local_ip) - 1);
    strncpy(s->server_ip, server_ip, sizeof(s->server_ip) - 1);

    s->server_port = server_port;
    if (local_port > 0) {
        s->local_port = local_port;
    } else {
        s->local_port = (uint16_t)(40000 + rand() % 10000);
    }
    s->seq = (uint32_t)rand();

    printf("[*] Puerto local cliente: %u\n", s->local_port);
    printf("[*] Regla iptables recomendada para evitar RST del kernel:\n");
    printf("    sudo iptables -A OUTPUT -p tcp --sport %u --tcp-flags RST RST -j DROP\n\n", s->local_port);

    s->sockfd = create_raw_socket(capture_interface);
    return s->sockfd < 0 ? -1 : 0;
}

int tcp_session_handshake(tcp_session_t *s, int timeout_sec)
{
    uint8_t packet[MAX_PACKET_SIZE];
    tcp_header_t recv_tcph;
    int recv_payload_len = 0;

    printf("════════════════════ TCP SESSION ════════════════════\n");

    /* 1. Cliente → Servidor : SYN */
    tcp_log_segment("Client → Server", TCP_SYN, s->seq, 0, NULL, 0);

    int len = build_packet(packet, s, TCP_SYN, NULL, 0);
    if (send_packet(s->sockfd, packet, len, s->server_ip) < 0) return -1;

    /* 2. Servidor → Cliente : SYN + ACK */
    time_t start = time(NULL);
    int found_synack = 0;
    while (difftime(time(NULL), start) < timeout_sec) {
        if (receive_next_tcp_packet(s, &recv_tcph, NULL, 0, &recv_payload_len, 1) != 0)
            continue;

        if ((recv_tcph.flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            found_synack = 1;
            break;
        }
    }

    if (!found_synack) {
        fprintf(stderr, "[!] Timeout esperando SYN+ACK del servidor\n");
        return -1;
    }

    uint32_t server_seq = ntohl(recv_tcph.seq_num);
    uint32_t server_ack = ntohl(recv_tcph.ack_num);

    tcp_log_segment("Server → Client", recv_tcph.flags, server_seq, server_ack, NULL, 0);

    s->seq++;                 /* SYN saliente consume 1 */
    s->ack = server_seq + 1;  /* SYN entrante consume 1 */

    /* 3. Cliente → Servidor : ACK */
    tcp_log_segment("Client → Server", TCP_ACK, s->seq, s->ack, NULL, 0);

    return send_control(s, TCP_ACK) < 0 ? -1 : 0;
}

int tcp_session_send_message(tcp_session_t *s, const char *message)
{
    char buffer[TCP_SESSION_MAX_MSG + 2];
    size_t len = strlen(message);

    if (len > TCP_SESSION_MAX_MSG - 1) len = TCP_SESSION_MAX_MSG - 1;

    memcpy(buffer, message, len);
    if (len == 0 || buffer[len - 1] != '\n') buffer[len++] = '\n';
    buffer[len] = '\0';

    /* 4. Cliente → Servidor : PSH + ACK + DATA */
    tcp_log_segment("Client → Server", TCP_PSH | TCP_ACK, s->seq, s->ack, (const uint8_t *)buffer, len);

    uint8_t packet[MAX_PACKET_SIZE];
    int packet_len = build_packet(packet, s, TCP_PSH | TCP_ACK, buffer, len);
    if (send_packet(s->sockfd, packet, packet_len, s->server_ip) < 0) return -1;

    s->seq += (uint32_t)len;

    tcp_header_t recv_tcph;
    uint8_t payload[TCP_SESSION_MAX_MSG + 1];
    int payload_len = 0;
    int got_echo = 0;
    int got_fin = 0;
    int is_exit = (strstr(message, "EXIT") != NULL);

    time_t start = time(NULL);
    int timeout_sec = 8;

    while (difftime(time(NULL), start) < timeout_sec) {
        if (receive_next_tcp_packet(s, &recv_tcph, payload, sizeof(payload), &payload_len, 1) != 0) {
            if (got_echo && !is_exit) {
                break;
            }
            continue;
        }

        uint32_t seq = ntohl(recv_tcph.seq_num);
        uint32_t ack = ntohl(recv_tcph.ack_num);
        uint8_t flags = recv_tcph.flags;

        /* 5. Servidor → Cliente : ACK puro */
        if (payload_len == 0 && !(flags & TCP_FIN)) {
            tcp_log_segment("Server → Client", flags, seq, ack, NULL, 0);
            continue;
        }

        /* 6. Servidor → Cliente : PSH + ACK + DATA (eco) */
        if (payload_len > 0) {
            payload[payload_len] = '\0';
            tcp_log_segment("Server → Client", flags, seq, ack, payload, (size_t)payload_len);
            s->ack += (uint32_t)payload_len;

            /* 7. Cliente → Servidor : ACK */
            tcp_log_segment("Client → Server", TCP_ACK, s->seq, s->ack, NULL, 0);
            send_control(s, TCP_ACK);
            got_echo = 1;
        }

        /* 8. Servidor → Cliente : FIN + ACK (cierre Opción 1) */
        if (flags & TCP_FIN) {
            printf("──────────────────── CLOSE ─────────────────────────\n");
            tcp_log_segment("Server → Client", flags, seq, ack, NULL, 0);
            s->ack += 1;

            /* 9. Cliente → Servidor : ACK */
            tcp_log_segment("Client → Server", TCP_ACK, s->seq, s->ack, NULL, 0);
            send_control(s, TCP_ACK);

            got_fin = 1;
            break;
        }

        /* Si ya recibimos el eco y el mensaje no era EXIT, chequeo breve */
        if (got_echo && !is_exit) {
            if (receive_next_tcp_packet(s, &recv_tcph, payload, sizeof(payload), &payload_len, 1) == 0) {
                uint32_t s_seq = ntohl(recv_tcph.seq_num);
                uint32_t s_ack = ntohl(recv_tcph.ack_num);
                uint8_t s_flags = recv_tcph.flags;
                if (s_flags & TCP_FIN) {
                    printf("──────────────────── CLOSE ─────────────────────────\n");
                    tcp_log_segment("Server → Client", s_flags, s_seq, s_ack, NULL, 0);
                    s->ack += 1;
                    tcp_log_segment("Client → Server", TCP_ACK, s->seq, s->ack, NULL, 0);
                    send_control(s, TCP_ACK);
                    got_fin = 1;
                }
            }
            break;
        }
    }

    if (got_fin) {
        /* 10. Cliente → Servidor : FIN + ACK */
        tcp_log_segment("Client → Server", TCP_FIN | TCP_ACK, s->seq, s->ack, NULL, 0);
        send_control(s, TCP_FIN | TCP_ACK);
        s->seq += 1;

        /* 11. Servidor → Cliente : ACK */
        if (receive_next_tcp_packet(s, &recv_tcph, NULL, 0, &payload_len, 5) == 0) {
            uint32_t final_seq = ntohl(recv_tcph.seq_num);
            uint32_t final_ack = ntohl(recv_tcph.ack_num);
            tcp_log_segment("Server → Client", recv_tcph.flags, final_seq, final_ack, NULL, 0);
        }

        printf("══════════════════ CONNECTION CLOSED ═══════════════\n");
        close_raw_socket(s->sockfd);
        s->sockfd = -1;
        return 1;
    }

    if (!got_echo) {
        fprintf(stderr, "[!] Timeout esperando respuesta del servidor\n");
        return -1;
    }

    return 0;
}

int tcp_session_drain_until_close(tcp_session_t *s, char *out_buf,
                                  size_t out_buf_size, int timeout_sec)
{
    (void)s;
    (void)out_buf;
    (void)out_buf_size;
    (void)timeout_sec;
    return 0;
}

void tcp_session_close(tcp_session_t *s, int timeout_sec)
{
    if (s->sockfd < 0) return;

    printf("──────────────────── CLOSE ─────────────────────────\n");
    /* Opción 2: Cliente inicia el cierre */
    /* 8. Cliente → Servidor : FIN + ACK */
    tcp_log_segment("Client → Server", TCP_FIN | TCP_ACK, s->seq, s->ack, NULL, 0);
    send_control(s, TCP_FIN | TCP_ACK);
    s->seq += 1;

    tcp_header_t recv_tcph;
    int payload_len = 0;

    /* 9. Servidor → Cliente : ACK */
    if (receive_next_tcp_packet(s, &recv_tcph, NULL, 0, &payload_len, timeout_sec) == 0) {
        tcp_log_segment("Server → Client", recv_tcph.flags, ntohl(recv_tcph.seq_num), ntohl(recv_tcph.ack_num), NULL, 0);
    }

    /* 10. Servidor → Cliente : FIN + ACK */
    if (receive_next_tcp_packet(s, &recv_tcph, NULL, 0, &payload_len, timeout_sec) == 0) {
        tcp_log_segment("Server → Client", recv_tcph.flags, ntohl(recv_tcph.seq_num), ntohl(recv_tcph.ack_num), NULL, 0);
        s->ack = ntohl(recv_tcph.seq_num) + 1;
        /* 11. Cliente → Servidor : ACK */
        tcp_log_segment("Client → Server", TCP_ACK, s->seq, s->ack, NULL, 0);
        send_control(s, TCP_ACK);
    }

    printf("══════════════════ CONNECTION CLOSED ═══════════════\n");
    close_raw_socket(s->sockfd);
    s->sockfd = -1;
}

void tcp_session_abort(tcp_session_t *s)
{
    if (s->sockfd >= 0) {
        close_raw_socket(s->sockfd);
        s->sockfd = -1;
    }
}