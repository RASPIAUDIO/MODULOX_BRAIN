from pathlib import Path
import sys
import struct
import binascii
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from release import ROOT, DATA_PARTITIONS, validate_layout, boot_a_metadata
from migrate_ab import validate_backup

class MigrationTest(unittest.TestCase):
    def test_layout_preserves_every_data_address(self):
        table = validate_layout((ROOT / "build/partition_table/partition-table.bin").read_bytes())
        self.assertEqual(table["ota_0"]["size"], 1572864)
        self.assertEqual(table["ota_1"]["size"], 1572864)
        self.assertEqual(table["samples"]["size"], 10 * 1024 * 1024)

    def test_initial_metadata_requires_confirmation_and_has_correct_crc(self):
        data = boot_a_metadata()
        self.assertEqual(len(data), 8192)
        self.assertEqual(struct.unpack_from("<I", data, 0)[0], 1)
        self.assertEqual(struct.unpack_from("<I", data, 24)[0], 0)
        self.assertEqual(struct.unpack_from("<I", data, 28)[0], binascii.crc32(data[:4], 0xffffffff) & 0xffffffff)
        self.assertEqual(data[4096:], b"\xff" * 4096)

    def test_unknown_or_overlapping_device_is_refused(self):
        with self.assertRaises(ValueError): validate_backup(b"\xff" * 10)
        data = bytearray(b"\xff" * 0x1000000)
        entries = {**DATA_PARTITIONS, "factory": (0x10000, 0x100000)}
        for index, (name, (offset, size)) in enumerate(entries.items()):
            struct.pack_into("<HBBII16sI", data, 0x8000 + index * 32, 0x50aa, 0 if name == "factory" else 1, 0, offset, size, name.encode(), 0)
        data[0x10000] = 0xe9
        validate_backup(data)
        data[0x1f0000] = 0
        with self.assertRaises(ValueError): validate_backup(data)

if __name__ == "__main__": unittest.main()
