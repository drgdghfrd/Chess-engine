#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
ANDROID = ROOT / "android"
PROPS = ANDROID / "gradle/wrapper/gradle-wrapper.properties"
GRADLEW = ANDROID / "gradlew"
GRADLEW_BAT = ANDROID / "gradlew.bat"
BUILD_APK = ANDROID / "build_apk.sh"

EXPECTED_DIST = "544c35d6bd849ae8a5ed0bcea39ba677dc40f49df7d1835561582da2009b961d"
EXPECTED_VERSION = "8.7"

def check(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)

props = PROPS.read_text()
gradlew = GRADLEW.read_text()
gradlew_bat = GRADLEW_BAT.read_text()

check(f"gradle-{EXPECTED_VERSION}-bin.zip" in props, "Gradle distribution is not pinned to 8.7")
check(f"distributionSha256Sum={EXPECTED_DIST}" in props, "Gradle distribution SHA-256 is wrong")
check("org.gradle.wrapper.GradleWrapperMain" in gradlew, "gradlew does not delegate to GradleWrapperMain")
check("org.gradle.wrapper.GradleWrapperMain" in gradlew_bat, "gradlew.bat does not delegate to GradleWrapperMain")
check("services.gradle.org/distributions" not in gradlew, "gradlew still contains the custom distribution downloader")
check("Invoke-WebRequest" not in gradlew_bat, "gradlew.bat still contains the custom distribution downloader")
check(GRADLEW.stat().st_mode & 0o111, "gradlew is not executable in the source package")
check(BUILD_APK.exists() and (BUILD_APK.stat().st_mode & 0o111), "build_apk.sh is missing or not executable")
check(not (ANDROID / "gradle/wrapper/gradle-wrapper.jar.sha256").exists(), "stale wrapper JAR checksum manifest remains")

print("v1.07 Gradle Wrapper tests: PASS")
