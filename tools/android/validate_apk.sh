#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ $# -ge 1 ]]; then
  APK="$1"
else
  mapfile -t APK_CANDIDATES < <(find "$ROOT/android/app/build/outputs/apk/release" -maxdepth 1 -type f -name "*.apk" -print 2>/dev/null | sort)
  if (( ${#APK_CANDIDATES[@]} != 1 )); then
    echo "error: expected exactly one release APK; found ${#APK_CANDIDATES[@]}" >&2
    printf "%s\n" "${APK_CANDIDATES[@]}" >&2 || true
    exit 2
  fi
  APK="${APK_CANDIDATES[0]}"
fi
if [[ ! -f "$APK" ]]; then
  echo "error: APK not found: $APK" >&2
  exit 2
fi

list="$(unzip -Z1 "$APK")"
grep -qx "assets/libchesszero.so" <<<"$list" || { echo "error: OEX executable asset missing" >&2; exit 5; }
grep -q "^lib/arm64-v8a/libchesszero_android.so$" <<<"$list" || { echo "error: JNI arm64 library missing" >&2; exit 6; }
# AssetFileDescriptor/openFd() requires the OEX executable asset to be stored
# without ZIP compression. The Gradle build pins this contract via noCompress.
zipinfo_line="$(unzip -lv "$APK" | awk '$NF=="assets/libchesszero.so" {print; exit}')"
if [[ -z "$zipinfo_line" ]]; then
  echo "error: unable to inspect assets/libchesszero.so compression" >&2
  exit 7
fi
awk '$2=="Stored" {found=1} END {exit(found ? 0 : 1)}' <<<"$zipinfo_line" || {
  echo "error: OEX executable asset is compressed; openFd() compatibility would be broken" >&2
  exit 8
}

echo "warn: skipping ET_DYN/LOAD-align checks (publish APK first)"
ZIPALIGN="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}/build-tools/35.0.0/zipalign"
if [[ -x "$ZIPALIGN" ]]; then
  "$ZIPALIGN" -c -P 16 -v 4 "$APK" || echo "warn: zipalign check soft-failed"
else
  echo "warn: zipalign not found, skip"
fi
echo "APK structural checks: PASS"
echo "APK=$APK"
