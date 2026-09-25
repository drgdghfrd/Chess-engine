#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "android" / "app" / "build.gradle.kts"
ANDROID_CMAKE = ROOT / "android" / "CMakeLists.txt"
BUILD = ROOT / "android" / "build_apk.sh"
LOCK = ROOT / "third_party" / "Fathom" / "SOURCE.lock"
GRADLE = ROOT / "android" / "gradle" / "wrapper" / "gradle-wrapper.properties"
WRAPPER = ROOT / "android" / "gradlew"
WRAPPER_BAT = ROOT / "android" / "gradlew.bat"
WRAPPER_JAR = ROOT / "android" / "gradle" / "wrapper" / "gradle-wrapper.jar"
VERSION = ROOT / "src" / "Version.h"

def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    return cond

checks = []
s_app = APP.read_text()
s_cmake = ANDROID_CMAKE.read_text()
s_build = BUILD.read_text()
s_lock = LOCK.read_text()
s_gradle = GRADLE.read_text()
s_wrapper = WRAPPER.read_text()
s_wrapper_bat = WRAPPER_BAT.read_text()
s_version = VERSION.read_text()

checks.append(check('compileSdk = 35' in s_app, 'compileSdk 35'))
checks.append(check('versionCode = 109' in s_app and 'versionName = "1.0.9"' in s_app, 'Android versionCode/versionName 1.0.9'))
checks.append(check('kVersion = "1.0.9"' in s_version, 'engine version 1.0.9'))
checks.append(check('targetSdk = 35' in s_app, 'targetSdk 35'))
checks.append(check('ndkVersion = "29.0.14206865"' in s_app, 'NDK r29 pinned'))
checks.append(check('CHESSZERO_REQUIRE_VENDORED_FATHOM "Require the vendored Fathom source tree; never FetchContent" ON' in s_cmake, 'Android Fathom vendor-required default'))
checks.append(check('Android NDK 29.0.14206865 is required' in s_build, 'release script checks NDK r29'))
checks.append(check('JDK 17 is required' in s_build, 'release script checks JDK 17'))
checks.append(check('distributionUrl=' in s_gradle and 'gradle-8.7-bin.zip' in s_gradle, 'Gradle 8.7 distribution pinned'))
checks.append(check('distributionSha256Sum=544c35d6bd849ae8a5ed0bcea39ba677dc40f49df7d1835561582da2009b961d' in s_gradle, 'Gradle distribution checksum pinned'))
checks.append(check('org.gradle.wrapper.GradleWrapperMain' in s_wrapper and 'org.gradle.wrapper.GradleWrapperMain' in s_wrapper_bat, 'standard Wrapper launcher delegates to WrapperMain'))
checks.append(check('services.gradle.org/distributions' not in s_wrapper and 'Invoke-WebRequest' not in s_wrapper_bat, 'custom bootstrap downloaders removed'))
checks.append(check(__import__('hashlib').sha256(WRAPPER_JAR.read_bytes()).hexdigest() == 'cb0da6751c2b753a16ac168bb354870ebb1e162e9083f116729cec9c781156b8', 'official Gradle 8.7 Wrapper JAR checksum'))
checks.append(check('src/tbprobe.c' in s_lock and 'src/tbprobe.h' in s_lock and 'src/tbchess.c' in s_lock, 'Fathom source lock present'))
checks.append(check((ROOT / 'third_party' / 'Fathom' / 'README.txt').exists(), 'Fathom vendor instructions present'))
checks.append(check((ROOT / 'third_party' / 'Fathom' / 'LICENSE').exists(), 'Fathom MIT license present'))
checks.append(check(not (ROOT / 'build-linux').exists(), 'clean source tree has no build-linux artifact'))

raise SystemExit(0 if all(checks) else 1)
