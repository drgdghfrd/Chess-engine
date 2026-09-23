#!/usr/bin/env bash
set -euo pipefail

FILE="${1:-}"
if [[ -z "$FILE" || ! -f "$FILE" ]]; then
  echo "usage: $0 <ELF-file>" >&2
  exit 2
fi

READELF="${READELF:-readelf}"
if ! command -v "$READELF" >/dev/null 2>&1; then
  echo "error: readelf not found; set READELF to llvm-readelf/readelf" >&2
  exit 3
fi

TYPE="$($READELF -h "$FILE" | awk '/Type:/{print $2}')"
if [[ "$TYPE" != "DYN" ]]; then
  echo "error: $FILE is not ET_DYN/PIE (type=$TYPE)" >&2
  exit 4
fi

bad=0
while IFS= read -r line; do
  align="$(awk '{print $NF}' <<<"$line")"
  case "$align" in
    0x4000|0X4000|0x8000|0X8000|0x10000|0X10000) ;;
    *) echo "error: PT_LOAD alignment is $align in $FILE (need >= 16 KiB)" >&2; bad=1 ;;
  esac
done < <($READELF -lW "$FILE" | awk '$1=="LOAD" {print}')

if (( bad )); then exit 5; fi
echo "16K+ ELF alignment check: PASS ($FILE)"
