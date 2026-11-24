from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("production", Path(__file__).resolve().parents[1] / "tools/flash_production.py")
production = importlib.util.module_from_spec(spec)
spec.loader.exec_module(production)

class ProductionImageTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.image = bytearray(b"\xff" * production.FLASH_BYTES)
        self.image[0] = 0xe9; struct.pack_into("<H", self.image, 12, 9)
        (self.root / "image.bin").write_bytes(self.image)
        self.manifest = {"format": "modulox.full-flash", "chip": "esp32s3", "flashBytes": production.FLASH_BYTES,
            "offset": 0, "size": len(self.image), "sha256": hashlib.sha256(self.image).hexdigest(), "file": "image.bin"}
        self.path = self.root / "manifest.json"
        self.save()
        self.calls = []
    def tearDown(self): self.directory.cleanup()
    def save(self): self.path.write_text(json.dumps(self.manifest))
    def tool(self, port, baud, *command):
        self.calls.append(command)
        return "Secure Boot: Disabled\nFlash Encryption: Disabled\n" if command[0] == "get_security_info" else "Detected flash size: 16MB\nFeatures: Embedded PSRAM 8MB (AP_3v3)\n"
    def test_check_only_never_writes(self):
        production.program(self.path, "COM_TEST", run=self.tool)
        self.assertEqual([c[0] for c in self.calls], ["get_security_info", "flash_id"])
    def test_write_uses_zero_and_verifies_full_file(self):
        production.program(self.path, "COM_TEST", write=True, run=self.tool)
        self.assertEqual(self.calls[2][0], "write_flash")
        self.assertEqual(self.calls[2][-2:], ("0x0", self.root / "image.bin"))
        self.assertEqual(self.calls[3], ("verify_flash", "0x0", self.root / "image.bin"))
    def test_corrupt_image_prevents_device_access(self):
        self.manifest["sha256"] = "0" * 64; self.save()
        with self.assertRaisesRegex(ValueError, "SHA-256"): production.program(self.path, "COM_TEST", write=True, run=self.tool)
        self.assertFalse(self.calls)
    def test_incompatible_targets_never_write(self):
        for response in ["Secure Boot: Enabled\nFlash Encryption: Disabled", "Secure Boot: Disabled\nFlash Encryption: Enabled",
                         "Secure Boot: Disabled\nFlash Encryption: Disabled\nDetected flash size: 8MB\nEmbedded PSRAM 8MB",
                         "Secure Boot: Disabled\nFlash Encryption: Disabled\nDetected flash size: 16MB\nEmbedded PSRAM 2MB"]:
            self.calls.clear()
            def incompatible(port, baud, *command): self.calls.append(command); return response
            with self.assertRaises(ValueError): production.program(self.path, "COM_TEST", write=True, run=incompatible)
            self.assertTrue(all(c[0] not in ("write_flash", "erase_flash") for c in self.calls))

if __name__ == "__main__": unittest.main()
