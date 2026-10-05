#!/usr/bin/env bash
# The AVR cost of Watch, OnSync and OnChange against bare Data (and the harness alone): flash, RAM, sizeof, cycles for eight sets.
# Needs avr-g++, avr-size and simavr; the HAPI checkout next to OneData.
set -e
cd "$(dirname "$0")"
INC="-I ../include -I ../../HAPI/include"
OUT=$(mktemp -d); trap 'rm -rf "$OUT"' EXIT
name=("harness alone" "Data" "Watch (loop tests changed, syncs)" "Watch + OnSync" "Data + OnChange")
printf '%-36s %8s %8s %8s %8s\n' variant "flash B" "ram B" "sizeof" cycles
for v in 0 1 2 3 4; do
  avr-g++ -std=gnu++17 -mmcu=atmega328p -Os -ffunction-sections -fdata-sections -Wl,--gc-sections -DVARIANT=$v $INC avr_onsync.cpp -o "$OUT/v$v.elf"
  sz=$(avr-size -C --mcu=atmega328p "$OUT/v$v.elf" | awk '/^Program:/{p=$2}/^Data:/{d=$2}END{print p" "d}')
  o=$(timeout 30 simavr -m atmega328p -f 16000000 "$OUT/v$v.elf" 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | tr -d '\r' | sed 's/\.$//')
  printf '%s' "$o" | grep -q '^OK' || { echo "V$v: WRONG or no result: $(printf '%s' "$o" | head -c 200)"; exit 1; }
  printf '%-36s %8s %8s %8s %8s\n' "${name[$v]}" $sz "$(printf '%s' "$o" | awk '/^SIZEOF/{print $2}')" "$(printf '%s' "$o" | awk '/^CYC/{print $2}')"
done
