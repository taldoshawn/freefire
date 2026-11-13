#!/usr/bin/env python3
# Python 3.11 / patch_apk.py / GitHub Actions Linux / original complete Android APK
"""Embed the NOX ARM64 visual module in a complete APK.

This does not discover or connect to Free Fire's entity/camera memory.
"""
import argparse
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
import xml.etree.ElementTree as ET

ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)


def run(*args: str) -> None:
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), check=True)


def verify_original(apk: Path) -> None:
    if not apk.is_file():
        raise FileNotFoundError(f"APK não encontrado: {apk}")
    with zipfile.ZipFile(apk) as z:
        files = set(z.namelist())
        mandatory = {"AndroidManifest.xml", "resources.arsc", "classes.dex"}
        missing = mandatory - files
        if missing:
            raise ValueError(f"APK incompleto, faltando: {', '.join(sorted(missing))}")
        if not any(s.startswith("lib/arm64-v8a/") for s in files):
            raise ValueError("APK sem bibliotecas ARM64 em lib/arm64-v8a/")
        if not any(s.startswith("assets/bin/Data/") for s in files):
            raise ValueError("APK sem recursos Unity em assets/bin/Data/")
        print(f"APK validado: {len(files)} entradas", flush=True)


def inject_provider(decoded: Path) -> str:
    manifest = decoded / "AndroidManifest.xml"
    tree = ET.parse(manifest)
    root = tree.getroot()
    package = root.attrib.get("package")
    if not package:
        raise ValueError("O manifesto decodificado não informa package")
    application = root.find("application")
    if application is None:
        raise ValueError("O manifesto não tem <application>")
    attr_name = f"{{{ANDROID_NS}}}name"
    for node in application.findall("provider"):
        if node.get(attr_name) == "com.nox.esp.BootstrapProvider":
            raise ValueError("O APK já contém o BootstrapProvider")
    node = ET.SubElement(application, "provider")
    node.set(attr_name, "com.nox.esp.BootstrapProvider")
    node.set(f"{{{ANDROID_NS}}}authorities", package + ".nox.bootstrap")
    node.set(f"{{{ANDROID_NS}}}exported", "false")
    node.set(f"{{{ANDROID_NS}}}enabled", "true")
    tree.write(manifest, encoding="utf-8", xml_declaration=True)
    return package


def next_dex_name(decoded: Path) -> str:
    indices = []
    for path in decoded.glob("classes*.dex"):
        match = re.fullmatch(r"classes(\d*)\.dex", path.name)
        if match:
            indices.append(int(match.group(1) or 1))
    if not indices:
        raise ValueError("apktool -s não preservou os DEX existentes")
    return f"classes{max(indices) + 1}.dex"


def patch(apk: Path, dex: Path, so: Path, apktool: Path, output: Path) -> None:
    verify_original(apk)
    for path in (dex, so, apktool):
        if not path.is_file():
            raise FileNotFoundError(f"Arquivo obrigatório não encontrado: {path}")
    with tempfile.TemporaryDirectory(prefix="nox-apk-") as tmp:
        base = Path(tmp)
        decoded = base / "decoded"
        run("java", "-Xmx4g", "-jar", str(apktool), "d", "-f", "-s",
            "-o", str(decoded), str(apk))

        package = inject_provider(decoded)
        dex_name = next_dex_name(decoded)
        shutil.copyfile(dex, decoded / dex_name)

        arch = decoded / "lib" / "arm64-v8a"
        arch.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(so, arch / "libnox_esp.so")
        shutil.rmtree(decoded / "lib" / "armeabi-v7a", ignore_errors=True)

        output.parent.mkdir(parents=True, exist_ok=True)
        run("java", "-Xmx4g", "-jar", str(apktool), "b",
            str(decoded), "-o", str(output))

        with zipfile.ZipFile(output) as result:
            names = set(result.namelist())
        required = {dex_name, "lib/arm64-v8a/libnox_esp.so",
                    "AndroidManifest.xml"}
        if not required <= names:
            raise RuntimeError(f"APK gerado sem: {sorted(required - names)}")
        print(f"OK: package={package}, DEX={dex_name}, saída={output}")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--apk", required=True, type=Path)
    p.add_argument("--dex", required=True, type=Path)
    p.add_argument("--so", required=True, type=Path)
    p.add_argument("--apktool", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    args = p.parse_args()
    patch(*(getattr(args, k) for k in
            ("apk", "dex", "so", "apktool", "output")))


if __name__ == "__main__":
    main()
