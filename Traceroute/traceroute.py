#!/usr/bin/env python3
"""Educational traceroute using manually built IPv4/UDP probes and raw ICMP."""
import argparse, select, socket, struct, time

PAYLOAD = b"traceroute-probe"

def checksum(data):
    if len(data) % 2: data += b"\0"
    total = sum((data[i] << 8) + data[i + 1] for i in range(0, len(data), 2))
    while total >> 16: total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff

def probe(src, dst, sport, dport, ttl, ident):
    udp_len = 8 + len(PAYLOAD)
    udp = struct.pack("!HHHH", sport, dport, udp_len, 0) + PAYLOAD
    pseudo = struct.pack("!4s4sBBH", socket.inet_aton(src), socket.inet_aton(dst), 0, socket.IPPROTO_UDP, udp_len)
    udp = udp[:6] + struct.pack("!H", checksum(pseudo + udp)) + udp[8:]
    header = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + udp_len, ident, 0, ttl, socket.IPPROTO_UDP, 0, socket.inet_aton(src), socket.inet_aton(dst))
    return header[:10] + struct.pack("!H", checksum(header)) + header[12:] + udp

def matching(reply, sport, dport):
    if len(reply) < 36: return None
    ihl = (reply[0] & 15) * 4
    if reply[9] != socket.IPPROTO_ICMP or len(reply) < ihl + 36: return None
    typ, code = reply[ihl], reply[ihl + 1]
    if typ not in (3, 11): return None
    quoted = reply[ihl + 8:]
    qihl = (quoted[0] & 15) * 4
    if len(quoted) < qihl + 8 or quoted[9] != socket.IPPROTO_UDP: return None
    qs, qd = struct.unpack("!HH", quoted[qihl:qihl + 4])
    if (qs, qd) != (sport, dport): return None
    return typ == 3 and code == 3

def main():
    p = argparse.ArgumentParser(description="Raw-socket educational traceroute")
    p.add_argument("host"); p.add_argument("-f", "--first-ttl", type=int, default=1)
    p.add_argument("-m", "--max-hops", type=int, default=64); p.add_argument("-q", "--probes", type=int, default=3)
    p.add_argument("-w", "--timeout", type=int, default=3000, help="timeout per probe in milliseconds")
    p.add_argument("-i", "--pause", type=int, default=100, help="pause between probes in milliseconds")
    a = p.parse_args(); dst = socket.gethostbyname(a.host); temp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); temp.connect((dst, 33434)); src = temp.getsockname()[0]; temp.close()
    send = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_UDP); send.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)
    recv = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP); sport = 40000 + (send.fileno() % 20000)
    print(f"traceroute to {a.host} ({dst}), {a.max_hops} hops max, {20 + 8 + len(PAYLOAD)} byte packets")
    for ttl in range(a.first_ttl, a.max_hops + 1):
        print(f"{ttl:2d} ", end="", flush=True); reached = False; shown = None
        for n in range(a.probes):
            dport = 33434 + ttl * a.probes + n; packet = probe(src, dst, sport, dport, ttl, ttl * 100 + n); start = time.monotonic(); send.sendto(packet, (dst, dport)); result = None; addr = None
            while (time.monotonic() - start) * 1000 < a.timeout:
                ready, _, _ = select.select([recv], [], [], min(0.1, a.timeout / 1000));
                if not ready: continue
                data, peer = recv.recvfrom(2048); result = matching(data, sport, dport)
                if result is not None: addr = peer[0]; break
            if result is None: print("* ", end="")
            else:
                if shown != addr:
                    try: name = socket.gethostbyaddr(addr)[0]
                    except socket.herror: name = addr
                    print(f"{name} ({addr}) ", end=""); shown = addr
                print(f"{(time.monotonic() - start) * 1000:.3f} ms ", end=""); reached |= result
            if n + 1 < a.probes: time.sleep(a.pause / 1000)
        print()
        if reached: break
    send.close(); recv.close()

if __name__ == "__main__": main()
