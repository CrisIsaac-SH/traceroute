CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

.PHONY: all clean

all: traceroute_c

traceroute_c: traceroute.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f traceroute_c
