/* Educational IPv4 traceroute: manual IP/UDP probes and raw ICMP replies. */
#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PAYLOAD "traceroute-probe"
#define DEFAULT_HOPS 64
#define DEFAULT_PROBES 3
#define DEFAULT_TIMEOUT 3000
#define DEFAULT_PAUSE 100

typedef struct { int first_ttl, max_hops, probes, timeout_ms, pause_ms; } options_t;

static uint16_t checksum(const uint8_t *data, size_t len) {
    uint32_t sum = 0;
    while (len > 1) { sum += ((uint16_t)data[0] << 8) | data[1]; data += 2; len -= 2; }
    if (len) sum += (uint16_t)data[0] << 8;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return (uint16_t)~sum;
}

static long long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void sleep_ms(int ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void usage(const char *p) {
    printf("Usage: %s [options] host\n\n"
           "  -f, --first-ttl N   initial TTL (default 1)\n"
           "  -m, --max-hops N    maximum hops (default 64)\n"
           "  -q, --probes N      probes per hop (default 3)\n"
           "  -w, --timeout MS    timeout per probe (default 3000)\n"
           "  -i, --pause MS      pause between probes (default 100)\n", p);
}

static int number(const char *s, int min, int max, const char *name) {
    char *end; long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < min || v > max) {
        fprintf(stderr, "Invalid %s: %s\n", name, s); exit(EXIT_FAILURE);
    }
    return (int)v;
}

static int parse(int argc, char **argv, options_t *o, const char **host) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 1; }
        if (!strcmp(a, "-f") || !strcmp(a, "--first-ttl")) { if (++i >= argc) return -1; o->first_ttl = number(argv[i], 1, 255, "first TTL"); continue; }
        if (!strcmp(a, "-m") || !strcmp(a, "--max-hops")) { if (++i >= argc) return -1; o->max_hops = number(argv[i], 1, 255, "max hops"); continue; }
        if (!strcmp(a, "-q") || !strcmp(a, "--probes")) { if (++i >= argc) return -1; o->probes = number(argv[i], 1, 20, "probes"); continue; }
        if (!strcmp(a, "-w") || !strcmp(a, "--timeout")) { if (++i >= argc) return -1; o->timeout_ms = number(argv[i], 1, 60000, "timeout"); continue; }
        if (!strcmp(a, "-i") || !strcmp(a, "--pause")) { if (++i >= argc) return -1; o->pause_ms = number(argv[i], 0, 60000, "pause"); continue; }
        if (a[0] == '-') { fprintf(stderr, "Unknown option: %s\n", a); return -1; }
        if (*host) { fprintf(stderr, "Only one destination is allowed\n"); return -1; }
        *host = a;
    }
    return *host ? 0 : -1;
}

static int build_probe(uint8_t *packet, struct in_addr src, struct in_addr dst,
                       uint16_t sport, uint16_t dport, int ttl, uint16_t id) {
    const size_t ip_len = 20, udp_len = 8, data_len = sizeof(PAYLOAD) - 1;
    memset(packet, 0, ip_len + udp_len + data_len);
    packet[0] = 0x45; packet[8] = (uint8_t)ttl; packet[9] = IPPROTO_UDP;
    uint16_t total = htons((uint16_t)(ip_len + udp_len + data_len));
    memcpy(packet + 2, &total, 2); uint16_t nid = htons(id); memcpy(packet + 4, &nid, 2);
    memcpy(packet + 12, &src.s_addr, 4); memcpy(packet + 16, &dst.s_addr, 4);
    uint16_t ip_sum = checksum(packet, ip_len); memcpy(packet + 10, &ip_sum, 2);
    uint8_t *udp = packet + ip_len; uint16_t n;
    n = htons(sport); memcpy(udp, &n, 2); n = htons(dport); memcpy(udp + 2, &n, 2);
    n = htons((uint16_t)(udp_len + data_len)); memcpy(udp + 4, &n, 2);
    memcpy(udp + 8, PAYLOAD, data_len);
    uint8_t pseudo[12 + udp_len + data_len]; memset(pseudo, 0, sizeof(pseudo));
    memcpy(pseudo, &src.s_addr, 4); memcpy(pseudo + 4, &dst.s_addr, 4); pseudo[9] = IPPROTO_UDP;
    memcpy(pseudo + 10, &n, 2); memcpy(pseudo + 12, udp, udp_len + data_len);
    uint16_t udp_sum = checksum(pseudo, sizeof(pseudo)); memcpy(udp + 6, &udp_sum, 2);
    return (int)(ip_len + udp_len + data_len);
}

static int matches_icmp(const uint8_t *buf, ssize_t len, uint16_t sport, uint16_t dport,
                        struct in_addr destination, int *reached) {
    if (len < 28 || (buf[0] & 0xf0) != 0x40 || buf[9] != IPPROTO_ICMP) return 0;
    size_t ihl = (buf[0] & 0x0f) * 4; if ((size_t)len < ihl + 8 + 28) return 0;
    const uint8_t *icmp = buf + ihl; uint8_t type = icmp[0], code = icmp[1];
    if (type != 11 && type != 3) return 0;
    const uint8_t *quoted = icmp + 8; size_t qihl = (quoted[0] & 0x0f) * 4;
    if (quoted[9] != IPPROTO_UDP || (size_t)len < ihl + 8 + qihl + 8) return 0;
    uint16_t qs, qd; memcpy(&qs, quoted + qihl, 2); memcpy(&qd, quoted + qihl + 2, 2);
    if (ntohs(qs) != sport || ntohs(qd) != dport) return 0;
    *reached = type == 3 && code == 3;
    memcpy(&destination.s_addr, buf + 12, 4); return 1;
}

int main(int argc, char **argv) {
    options_t o = {1, DEFAULT_HOPS, DEFAULT_PROBES, DEFAULT_TIMEOUT, DEFAULT_PAUSE};
    const char *host = NULL; int parsed = parse(argc, argv, &o, &host);
    if (parsed != 0) { if (parsed < 0) usage(argv[0]); return parsed < 0; }
    struct addrinfo hints = {0}, *res = NULL; hints.ai_family = AF_INET;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) { perror("getaddrinfo"); return 1; }
    struct sockaddr_in target = *(struct sockaddr_in *)res->ai_addr; freeaddrinfo(res);
    char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &target.sin_addr, ip, sizeof(ip));
    int route = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in route_dst = target;
    if (route < 0 || connect(route, (struct sockaddr *)&route_dst, sizeof(route_dst)) < 0) { perror("route socket"); return 1; }
    struct sockaddr_in local; socklen_t local_len = sizeof(local); getsockname(route, (struct sockaddr *)&local, &local_len); close(route);
    int sendfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP), recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sendfd < 0 || recvfd < 0) { perror("raw socket (run as root/CAP_NET_RAW)"); return 1; }
    int one = 1; setsockopt(sendfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    printf("traceroute to %s (%s), %d hops max, %d byte packets\n", host, ip, o.max_hops, (int)(20 + 8 + sizeof(PAYLOAD) - 1));
    uint16_t sport = (uint16_t)(40000 + getpid() % 20000); int done = 0;
    for (int ttl = o.first_ttl; ttl <= o.max_hops && !done; ttl++) {
        printf("%2d ", ttl); fflush(stdout); char shown[INET_ADDRSTRLEN] = ""; char name[NI_MAXHOST] = "";
        for (int probe = 0; probe < o.probes; probe++) {
            uint16_t dport = (uint16_t)(33434 + ttl * o.probes + probe); uint8_t packet[256];
            int plen = build_probe(packet, local.sin_addr, target.sin_addr, sport, dport, ttl, (uint16_t)(ttl * 100 + probe));
            struct sockaddr_in dst = target; long long started = now_ms();
            if (sendto(sendfd, packet, plen, 0, (struct sockaddr *)&dst, sizeof(dst)) < 0) { printf("send-error "); continue; }
            uint8_t reply[2048]; struct sockaddr_in responder; socklen_t responder_len = sizeof(responder); int reached = 0, matched = 0;
            while (now_ms() - started < o.timeout_ms) {
                fd_set set; FD_ZERO(&set); FD_SET(recvfd, &set); struct timeval tv = {0, 100000};
                if (select(recvfd + 1, &set, NULL, NULL, &tv) <= 0) continue;
                ssize_t n = recvfrom(recvfd, reply, sizeof(reply), 0, (struct sockaddr *)&responder, &responder_len);
                if (n > 0 && matches_icmp(reply, n, sport, dport, target.sin_addr, &reached)) { matched = 1; break; }
            }
            if (!matched) printf("* ");
            else { char addr[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &responder.sin_addr, addr, sizeof(addr));
                long long elapsed = now_ms() - started; if (shown[0] && strcmp(shown, addr) != 0) printf("%s ", addr);
                if (!shown[0]) { strncpy(shown, addr, sizeof(shown) - 1); if (getnameinfo((struct sockaddr *)&responder, responder_len, name, sizeof(name), NULL, 0, NI_NAMEREQD) != 0) strcpy(name, addr); printf("%s (%s) ", name, addr); }
                printf("%lld ms ", elapsed); if (reached) done = 1;
            }
            if (probe + 1 < o.probes) sleep_ms(o.pause_ms);
        }
        putchar('\n');
    }
    close(sendfd); close(recvfd); return 0;
}
