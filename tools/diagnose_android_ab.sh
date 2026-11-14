#!/usr/bin/env bash
set -uo pipefail
mkdir -p ab-output
apk=./ab-output/NOX-FF-OB55-runtime-probe.apk
echo "== ABI ==" | tee ab-output/summary.txt
adb shell getprop ro.product.cpu.abilist | tee -a ab-output/summary.txt
adb shell getprop ro.dalvik.vm.native.bridge | tee -a ab-output/summary.txt
echo "== INSTALL ==" | tee -a ab-output/summary.txt
adb install -r "$apk" 2>&1 | tee -a ab-output/summary.txt
for mode in overlay_enabled overlay_disabled; do
  echo "===== MODE $mode =====" | tee -a ab-output/summary.txt
  adb shell am force-stop com.dts.freefireth >/dev/null 2>&1 || true
  if [ "$mode" = overlay_disabled ]; then
    echo "Solicitando root ADB no emulador para alterar somente o componente de teste" | tee -a ab-output/summary.txt
    adb root 2>&1 | tee -a ab-output/summary.txt || true
    adb wait-for-device
    if ! adb shell pm disable --user 0 com.dts.freefireth/com.nox.esp.BootstrapProvider \
        2>&1 | tee -a ab-output/summary.txt; then
      echo "DESATIVACAO_FALHOU: este A/B nao e conclusivo" | tee -a ab-output/summary.txt
      continue
    fi
    adb shell dumpsys package com.dts.freefireth |
      grep -A10 -B2 BootstrapProvider | head -n 24 >> ab-output/summary.txt || true
  fi
  adb logcat -c || true
  adb shell am start -n com.dts.freefireth/.FFMainActivity \
    > "ab-output/$mode-launch.txt" 2>&1 || true
  sleep 48
  adb logcat -d -v threadtime > "ab-output/$mode-logcat.txt" 2>&1 || true
  adb shell screencap -p "/sdcard/nox-$mode.png" >/dev/null 2>&1 && \
    adb pull "/sdcard/nox-$mode.png" "ab-output/$mode-screen.png" >/dev/null 2>&1 || true
  adb shell dumpsys activity activities | grep 'topResumedActivity=' | tail -1 \
    | tee -a ab-output/summary.txt || true
  echo "Crash detection:" | tee -a ab-output/summary.txt
  grep -iE 'GC tried to mark invalid reference|Fatal signal|SIGABRT|com\.ano\.gshell\.AnoApplication|FATAL EXCEPTION|libndk_translation|NOX-RUNTIME' \
    "ab-output/$mode-logcat.txt" | tail -n 40 | tee -a ab-output/summary.txt || true
done
