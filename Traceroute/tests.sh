#!/usr/bin/env bash
set -u
set -o pipefail

cd "$(dirname "$0")"
make clean && make
./traceroute_c --help >/dev/null
python3 -m py_compile traceroute.py
perl -c traceroute.pl
printf 'Static checks passed. A live run requires Linux raw-socket privileges.\n'
