# Python 3.11 / tests/test_patch.py / GitHub Actions / APK manifest and DEX assembly checks
import tempfile
import unittest
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

from tools.patch_apk import inject_provider, next_dex_name, verify_original

ANDROID_NS = "http://schemas.android.com/apk/res/android"


class PatchTests(unittest.TestCase):
    def test_dex_slot(self):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            for name in ("classes.dex", "classes2.dex", "classes4.dex"):
                (folder / name).touch()
            self.assertEqual(next_dex_name(folder), "classes5.dex")

    def test_manifest_provider(self):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            (folder / "AndroidManifest.xml").write_text(
                '<manifest xmlns:android="http://schemas.android.com/apk/res/android" '
                'package="com.example.game"><application /></manifest>', encoding="utf8")
            self.assertEqual(inject_provider(folder), "com.example.game")
            root = ET.parse(folder / "AndroidManifest.xml").getroot()
            provider = root.find("./application/provider")
            self.assertEqual(provider.get(f"{{{ANDROID_NS}}}name"),
                             "com.nox.esp.BootstrapProvider")
            self.assertEqual(provider.get(f"{{{ANDROID_NS}}}exported"), "false")
            with self.assertRaises(ValueError):
                inject_provider(folder)

    def test_incomplete_apk_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "partial.apk"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("classes.dex", b"placeholder")
            with self.assertRaisesRegex(ValueError, "incompleto"):
                verify_original(path)


if __name__ == "__main__":
    unittest.main()
