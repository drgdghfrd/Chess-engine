#!/usr/bin/env python3
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent
manifest = root / 'app/src/main/AndroidManifest.xml'
engine = root / 'app/src/main/res/xml/enginelist.xml'
binpath = root / 'app/src/main/jniLibs/arm64-v8a/libultrachess.so'

ET.parse(manifest)
tree = ET.parse(engine)
r = tree.getroot()
assert r.tag == 'enginelist', r.tag
items = r.findall('engine')
assert items and items[0].get('name') == 'UltraChess v0.12'
assert items[0].get('filename') == 'libultrachess.so'
assert 'arm64-v8a' in (items[0].get('target') or '')
print('OEX XML/manifest: PASS')
print('ARM64 binary:', 'PRESENT' if binpath.exists() else 'MISSING (build with build_native_arm64.sh)')
