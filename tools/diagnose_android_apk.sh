#!/usr/bin/env bash
# Capture reproducible diagnostics without claiming emulator success proves device compatibility.
set -uo pipefail
mkdir -p test-output
{
  echo "===== Android emulator build ====="
  adb shell getprop ro.build.version.release
  adb shell getprop ro.build.version.sdk
  adb shell getprop ro.product.cpu.abilist
  adb shell getprop ro.dalvik.vm.native.bridge
  adb devices -l
} > test-output/emulator-info.txt 2>&1 || true
cat test-output/emulator-info.txt
adb logcat -c || true
echo "Comecando adb install..."
timeout 360s adb install -r test-output/NOX-FF-OB55-runtime-probe.apk > test-output/install.txt 2>&1
code=$?
echo "INSTALL_EXIT_CODE=$code" | tee -a test-output/diagnostico.txt
cat test-output/install.txt
if [ "$code" -eq 0 ]; then
  echo "Iniciando com monkey (launcher)..."
  timeout 35s adb shell monkey -p com.dts.freefireth -c android.intent.category.LAUNCHER 1 \
    > test-output/launch.txt 2>&1 || true
  cat test-output/launch.txt
  sleep 80
else
  echo "Instalacao falhou. Verifique ABI suportada e install.txt." >> test-output/diagnostico.txt
fi
adb logcat -d -v threadtime > test-output/smoke-logcat.txt 2>&1 || true
grep -Ei 'fatal exception|fatal signal|AndroidRuntime|am_crash|tombstone|com\.dts\.freefireth|nox|unity|il2cpp|crash|ANR|UnsatisfiedLinkError|VerifyError|ClassNotFoundException|PackageManager|install_failed|ABI' \
  test-output/smoke-logcat.txt | tail -n 800 > test-output/smoke-filtered.txt || true
adb shell dumpsys activity activities > test-output/activity.txt 2>&1 || true
adb shell dumpsys package com.dts.freefireth > test-output/package.txt 2>&1 || true
adb shell dumpsys dropbox --print > test-output/crash.txt 2>&1 || true
adb shell screencap -p /sdcard/nox-screen.png >/dev/null 2>&1 && \
  adb pull /sdcard/nox-screen.png test-output/screenshot.png >/dev/null 2>&1 || true
echo "===== Relevant system logs ====="
tail -n 140 test-output/smoke-filtered.txt || true
echo "===== Top activity ====="
grep -E 'mResumedActivity|topResumedActivity|mCurrentFocus|Hist #|ActivityRecord' \
  test-output/activity.txt | head -n 30 || true
