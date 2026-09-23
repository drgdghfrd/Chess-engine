#!/usr/bin/env bash
set -euo pipefail

ELF="${1:?usage: check_elf_16k.sh <elf> }"
if [[ ! -f "$ELF" ]]; then
  echo "missing ELF: $ELF" >&2
  exit 2
fi

command -v readelf >/dev/null || { echo "readelf is required" >&2; exit 2; }

bad=0
while read -r align; do
  [[ -n "$align" ]] || continue
  if [[ "$align" == 0x* ]]; then
    value=$((align))
  else
    value=$((16#$align))
  fi
  if (( value < 0x4000 )); then
    echo "FAIL: LOAD alignment $align is smaller than 0x4000 (16 KB)" >&2
    bad=1
  fi
done < <(readelf -lW "$ELF" | awk '$1=="LOAD" {print $NF}')

if (( bad )); then exit 1; fi
echo "16 KB ELF alignment: PASS ($ELF)"
