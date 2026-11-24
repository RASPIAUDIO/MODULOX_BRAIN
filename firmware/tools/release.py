"""Build the USB update file and the initial, data-preserving A/B migration kit."""
from pathlib import Path
import base64
import binascii
import hashlib
import json
import struct
import argparse
from board_profiles import profile, DEFAULT, PROFILES, data_partitions

ROOT = Path(__file__).resolve().parents[1]
SLOT = 0x180000
MODEL = "modulox-brain-s3"
LAYOUT = "ab-1.5m-v1"
DATA_PARTITIONS = {"nvs": (0x9000, 0x6000), "phy_init": (0xf000, 0x1000),
    "patches": (0x110000, 0x20000), "userpatches": (0x130000, 0x20000),
    "waves": (0x150000, 0xa0000), "samples": (0x200000, 0xa00000)}

def partitions(binary):
    result = {}
    for offset in range(0, len(binary) - 31, 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from("<HBBII16sI", binary, offset)
        if magic != 0x50aa:
            break
        name = label.split(b"\0")[0].decode("ascii")
        if name in result:
            raise ValueError("Duplicate partition label")
        result[name] = {"offset": start, "size": size, "type": kind, "subtype": subtype}
    return result

def validate_layout(binary, board=None):
    board = board or profile()
    table = partitions(binary)
    expected = {**data_partitions(board), "factory": (0x10000, 0x100000),
        "otadata": (0x1f0000, 0x2000), "ota_0": (board['ota0'], SLOT), "ota_1": (board['ota1'], SLOT)}
    for name, (offset, size) in expected.items():
        if name not in table or (table[name]["offset"], table[name]["size"]) != (offset, size):
            raise ValueError("Unsafe partition layout: " + name)
    assert table["ota_0"]["subtype"] == 0x10 and table["ota_1"]["subtype"] == 0x11
    assert table["otadata"]["type"] == 1 and table["otadata"]["subtype"] == 0
    ordered = sorted(table.values(), key=lambda item: item["offset"])
    for left, right in zip(ordered, ordered[1:]):
        if left["offset"] + left["size"] > right["offset"]:
            raise ValueError("Overlapping partitions")
    if ordered[-1]["offset"] + ordered[-1]["size"] > board['flashBytes']:
        raise ValueError("Layout exceeds board flash")
    return table

def boot_a_metadata():
    # ESP-IDF esp_ota_select_entry_t: seq, label[20], state, crc(seq).
    # NEW=0 makes the rollback-enabled bootloader require a successful self-test.
    binary = bytearray(b"\xff" * 0x2000)
    seq = struct.pack("<I", 1)
    struct.pack_into("<I", binary, 0, 1)
    struct.pack_into("<I", binary, 24, 0)
    struct.pack_into("<I", binary, 28, binascii.crc32(seq, 0xffffffff) & 0xffffffff)
    return bytes(binary)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', choices=PROFILES, default=DEFAULT)
    args = parser.parse_args()
    board = profile(args.model)
    build = ROOT / ('build' if args.model == DEFAULT else 'build-' + args.model)
    config = ROOT / ('sdkconfig' if args.model == DEFAULT else 'sdkconfig.' + args.model)
    if "MODULOX_ROLLBACK_TEST:BOOL=ON" in (build / "CMakeCache.txt").read_text():
        raise ValueError("Refusing to publish an intentionally failing rollback-test image")
    if "MODULOX_USB_DIAGNOSTICS:BOOL=ON" in (build / "CMakeCache.txt").read_text():
        raise ValueError("Refusing to publish a USB diagnostic image")
    image = (build / (board['project'] + '.bin')).read_bytes()
    table = (build / "partition_table/partition-table.bin").read_bytes()
    validate_layout(table, board)
    if not 288 <= len(image) <= SLOT or image[0] != 0xe9 or struct.unpack_from("<H", image, 12)[0] != 9 or struct.unpack_from("<I", image, 32)[0] != 0xabcd5432:
        raise ValueError("Invalid ESP32-S3 application")
    version = image[48:80].split(b"\0")[0].decode("ascii")
    if version != (ROOT / "version.txt").read_text().strip() or image[80:112].split(b"\0")[0].decode() != board['project']:
        raise ValueError("Unexpected application descriptor")
    if "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" not in config.read_text():
        raise ValueError("Rollback must be enabled")
    filename = f"modulox-{version}.bin"
    release = {"model": board['model'], "hardwareProfile": args.model, "chip": "esp32s3", "layout": board['layout'], "protocol": 1,
        "version": version, "size": len(image), "sha256": sha(image), "buildId": image[176:208].hex(),
        "file": filename, "notes": (ROOT / "RELEASE-NOTES.txt").read_text(encoding="utf-8").strip()}
    destination = ROOT / "releases" / version
    if args.model != DEFAULT: destination /= args.model
    destination.mkdir(parents=True, exist_ok=True)
    images = {filename: image, "bootloader.bin": (build / "bootloader/bootloader.bin").read_bytes(),
        "partition-table.bin": table, "boot-a.bin": boot_a_metadata()}
    for name, data in images.items():
        (destination / name).write_bytes(data)
    (destination / "latest.json").write_text(json.dumps(release, indent=2) + "\n", encoding="utf-8")
    bundle = {"format": "modulox.firmware", "version": 1, "release": release, "image": base64.b64encode(image).decode("ascii")}
    bundle_name = f"modulox-{version}.modulox-fw.json"
    (destination / bundle_name).write_text(json.dumps(bundle), encoding="utf-8")
    kit = {"format": "modulox.migration", "version": 1, "model": board['model'], "layout": board['layout'],
        "files": {name: {"sha256": sha(data), "size": len(data)} for name, data in images.items()}, "application": filename}
    (destination / "migration.json").write_text(json.dumps(kit, indent=2), encoding="utf-8")
    public = ROOT.parent / "editor/dist/firmware"
    if args.model != DEFAULT: public /= args.model
    public.mkdir(parents=True, exist_ok=True)
    for name in [filename, "latest.json", bundle_name]:
        (public / name).write_bytes((destination / name).read_bytes())
    print(f"Release {version}: {len(image)} bytes / {SLOT} bytes per slot, sha256={sha(image)}")
    print(destination)

if __name__ == "__main__":
    main()
