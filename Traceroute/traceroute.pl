#!/usr/bin/env perl
use strict;
use warnings;
use Socket qw(AF_INET SOCK_RAW SOCK_DGRAM IPPROTO_IP IPPROTO_UDP IPPROTO_ICMP IP_HDRINCL inet_ntoa pack_sockaddr_in unpack_sockaddr_in getaddrinfo getnameinfo NI_NAMEREQD);
use Time::HiRes qw(time usleep);

my ($first, $max, $probes, $timeout, $pause) = (1, 64, 3, 3000, 100); 
my $host;

while (@ARGV) { 
    my $a = shift; 
    if ($a eq '-h' || $a eq '--help') { 
        print "Usage: $0 [options] host\n-f first TTL -m max hops -q probes -w timeout ms -i pause ms\n"; 
        exit; 
    } 
    my %key = (
        '-f'=>\$first, '--first-ttl'=>\$first,
        '-m'=>\$max, '--max-hops'=>\$max,
        '-q'=>\$probes, '--probes'=>\$probes,
        '-w'=>\$timeout, '--timeout'=>\$timeout,
        '-i'=>\$pause, '--pause'=>\$pause
    ); 
    if (exists $key{$a}) { 
        die "Missing value for $a\n" unless @ARGV; 
        ${$key{$a}} = shift; 
        next; 
    } 
    die "Unknown option: $a\n" if $a =~ /^-/; 
    die "Only one host\n" if defined $host; 
    $host = $a; 
}

die "Missing host\n" unless defined $host;

# Resolución DNS
my ($gai_error, @addresses) = getaddrinfo($host, 0, {socktype=>SOCK_DGRAM, family=>AF_INET});
die "getaddrinfo: $gai_error\n" if $gai_error; 

my $dst_sockaddr = (grep { $_->{family} == AF_INET } @addresses)[0]{addr}; 
die "Could not resolve $host to IPv4\n" unless $dst_sockaddr;

# Uso explícito de unpack_sockaddr_in
my (undef, $dst_ip_packed) = unpack_sockaddr_in($dst_sockaddr);
my $dst_ip = inet_ntoa($dst_ip_packed); 

socket(my $tmp, AF_INET, SOCK_DGRAM, 0) or die "socket: $!"; 
connect($tmp, pack_sockaddr_in(33434, $dst_ip_packed)) or die "connect: $!"; 
my (undef, $local_ip_packed) = unpack_sockaddr_in(getsockname($tmp)); 
close $tmp;

socket(my $send, AF_INET, SOCK_RAW, IPPROTO_UDP) or die "raw socket: $! (run as root)"; 
setsockopt($send, IPPROTO_IP, IP_HDRINCL, pack('I', 1)) or die "IP_HDRINCL: $!"; 
socket(my $recv, AF_INET, SOCK_RAW, IPPROTO_ICMP) or die "ICMP socket: $!";

my $sport = 40000 + ($$ % 20000); 
print "traceroute to $host ($dst_ip), $max hops max, 44 byte packets\n";

sub csum { 
    my ($d) = @_; 
    $d .= "\0" if length($d) % 2; 
    my $s = 0; 
    $s += unpack('n', substr($d, $_, 2)) for (0 .. length($d)/2 - 1); 
    $s = ($s & 65535) + ($s >> 16) while $s >> 16; 
    return pack('n', (~$s) & 65535); 
}

sub packet { 
    my ($ttl, $sp, $dp, $id) = @_; 
    my $data = 'traceroute-probe'; 
    my $udp = pack('nnnn', $sp, $dp, 8 + length($data), 0) . $data; 
    my $pseudo = $local_ip_packed . $dst_ip_packed . pack('CCn', 0, IPPROTO_UDP, length($udp)); 
    my $uc = csum($pseudo . $udp); 
    substr($udp, 6, 2, $uc); 
    
    my $ip = pack('CCnnnCCnNN', 0x45, 0, 20 + length($udp), $id, 0, $ttl, IPPROTO_UDP, 0, unpack('N', $local_ip_packed), unpack('N', $dst_ip_packed)); 
    substr($ip, 10, 2, csum($ip)); 
    return $ip . $udp; 
}

for my $ttl ($first .. $max) { 
    printf('%2d ', $ttl); 
    my $done = 0; 
    
    for my $n (0 .. $probes - 1) { 
        my $dp = 33434 + ($ttl * $probes) + $n; 
        my $id = ($ttl * 100) + $n;
        my $start = time; 
        
        # Uso explícito de pack_sockaddr_in
        send($send, packet($ttl, $sport, $dp, $id), 0, pack_sockaddr_in($dp, $dst_ip_packed)); 
        
        my ($ok, $reached, $addr, $addr_packed); 
        
        while ((time - $start) * 1000 < $timeout) { 
            my $rin = ''; 
            vec($rin, fileno($recv), 1) = 1; 
            last unless select($rin, undef, undef, 0.1); 
            
            my $reply = ''; 
            recv($recv, $reply, 2048, 0); 
            next unless length($reply) >= 36; 
            
            my $ihl = (unpack('C', $reply) & 15) * 4; 
            next unless unpack('C', substr($reply, 9, 1)) == IPPROTO_ICMP; 
            
            my $type = unpack('C', substr($reply, $ihl, 1)); 
            next unless $type == 3 || $type == 11; 
            
            my $q = substr($reply, $ihl + 8); 
            my $qi = (unpack('C', $q) & 15) * 4; 
            next unless unpack('C', substr($q, 9, 1)) == IPPROTO_UDP; 
            
            my ($qs, $qd) = unpack('nn', substr($q, $qi, 4)); 
            next unless $qs == $sport && $qd == $dp; 
            
            # Guardamos la versión binaria empaquetada para evitar usar inet_aton luego
            $addr_packed = substr($reply, 12, 4);
            $addr = inet_ntoa($addr_packed); 
            $reached = ($type == 3); 
            $ok = 1; 
            last; 
        } 
        
        if (!$ok) { 
            print '* '; 
        } else { 
            if (!$addr) { 
                print "? "; 
            } else { 
                # Ahora pasamos directamente los 4 bytes seguros ($addr_packed)
                my ($dns_error, $resolved) = getnameinfo(pack_sockaddr_in(0, $addr_packed), NI_NAMEREQD); 
                my $name = (!$dns_error && $resolved) ? $resolved : $addr; 
                print "$name ($addr) " if $n == 0; 
                printf "%.3f ms ", (time - $start) * 1000; 
            } 
            $done ||= $reached; 
        } 
        usleep($pause * 1000) if $n + 1 < $probes; 
    } 
    print "\n"; 
    last if $done; 
}