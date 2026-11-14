#!/usr/bin/env python3
"""Create a control APK without the NOX overlay, for startup A/B diagnosis.

Does not bypass game signature, authentication or integrity protection.
The output is unsigned and must be signed for an emulator test.
"""
import argparse
import struct
import zipfile
from pathlib import Path

def iter_chunks(data):
    p = 8
    while p < len(data):
        typ, header, length = struct.unpack_from("<HHI", data, p)
        if length < header or length == 0 or p + length > len(data):
            raise ValueError("Invalid Android binary XML chunk")
        yield p, typ, length
        p += length
    if p != len(data):
        raise ValueError("Invalid XML size")

def without_nox_provider(data):
    if struct.unpack_from("<HHI", data, 0) != (3, 8, len(data)):
        raise ValueError("Expected Android binary XML")
    blocks = list(iter_chunks(data))
    candidates = []
    string_count = struct.unpack_from("<I", data, 16)[0]
    for j, (p, typ, length) in enumerate(blocks[:-1]):
        if typ != 0x0102 or length != 96:
            continue
        attr_start, attr_size, attr_count = struct.unpack_from("<HHH", data, p+24)
        if (attr_start, attr_size, attr_count) != (20, 20, 3):
            continue
        q, endtype, endlen = blocks[j+1]
        if q != p+length or endtype != 0x0103 or endlen != 24:
            continue
        if struct.unpack_from("<I", data, p+20)[0] != struct.unpack_from("<I", data, q+20)[0]:
            continue
        attr0_rawvalue = struct.unpack_from("<I", data, p+44)[0]
        if attr0_rawvalue == string_count-2:
            candidates.append((p, q+endlen))
    if len(candidates) != 1:
        raise ValueError(f"Expected one adjacent NOX provider pair; got {len(candidates)}")
    start, end = candidates[0]
    out = bytearray(data[:start] + data[end:])
    struct.pack_into("<I", out, 4, len(out))
    return bytes(out)

def strip(src, dst):
    with zipfile.ZipFile(src) as z, zipfile.ZipFile(dst, "w", allowZip64=True) as w:
        names = set(z.namelist())
        required = {"classes4.dex", "lib/arm64-v8a/libnox_esp.so"}
        if not required <= names:
            raise ValueError("NOX module is not present")
        manifest = without_nox_provider(z.read("AndroidManifest.xml"))
        for info in z.infolist():
            name = info.filename
            if name in required:
                continue
            if name.startswith("META-INF/") and name.upper().endswith((".RSA",".DSA",".EC",".SF")):
                continue
            if name == "AndroidManifest.xml":
                w.writestr(info, manifest)
            else:
                with z.open(info) as inp, w.open(info,"w") as out:
                    while True:
                        block = inp.read(1024*1024)
                        if not block:
                            break
                        out.write(block)
    with zipfile.ZipFile(dst) as z:
        assert "classes4.dex" not in z.namelist()
        assert "lib/arm64-v8a/libnox_esp.so" not in z.namelist()
        assert "lib/arm64-v8a/libil2cpp.so" in z.namelist()
        assert z.testzip() is None

if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("apk",type=Path)
    p.add_argument("out",type=Path)
    a=p.parse_args()
    strip(a.apk,a.out)
