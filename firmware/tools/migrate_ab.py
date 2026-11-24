"""One-time ROM migration. Never erases the chip or writes personal data partitions."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
import re
from datetime import datetime, timezone
from release import ROOT, DATA_PARTITIONS, partitions, validate_layout, boot_a_metadata, sha

def validate_backup(data):
    if len(data) != 0x1000000:
        raise ValueError("A complete 16 MiB flash backup is required")
    old = partitions(data[0x8000:0x9000])
    for name, (offset, size) in {**DATA_PARTITIONS, "factory": (0x10000, 0x100000)}.items():
        if name not in old or (old[name]["offset"], old[name]["size"]) != (offset, size):
            raise ValueError("Unknown device layout; refusing migration: " + name)
    for item in old.values():
        if item["offset"] < 0x1f2000 and item["offset"] + item["size"] > 0x1f0000:
            raise ValueError("OTA metadata space is already allocated; use USB update/recovery instead")
        if item["offset"] + item["size"] > 0xc00000:
            raise ValueError("OTA slot space is already allocated; migration is not needed")
    if data[0x10000] != 0xe9 or data[0x1f0000:0x1f2000] != b"\xff" * 0x2000:
        raise ValueError("Factory image missing or reserved space is not empty")
    return old

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--release", type=Path, default=ROOT / "releases" / (ROOT / "version.txt").read_text().strip())
    parser.add_argument("--backup", type=Path, help="Reuse a freshly read complete backup of THIS connected device")
    parser.add_argument("--check-only", action="store_true", help="Inspect the backup and migration kit without any flash writes")
    args = parser.parse_args()
    kit = json.loads((args.release / "migration.json").read_text())
    if kit.get("format") != "modulox.migration" or kit.get("layout") != "ab-1.5m-v1":
        raise ValueError("Invalid migration kit")
    for name, info in kit["files"].items():
        if Path(name).name != name or sha((args.release / name).read_bytes()) != info["sha256"]:
            raise ValueError("Corrupt migration kit")
    validate_layout((args.release / "partition-table.bin").read_bytes())
    if (args.release / "boot-a.bin").read_bytes() != boot_a_metadata():
        raise ValueError("Unexpected boot metadata")
    def tool(*command):
        subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
            "--baud", "921600", "--after", "no_reset", *map(str, command)], check=True)
    identity = subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
        "--after", "no_reset", "read_mac"], check=True, capture_output=True, text=True)
    match = re.search(r"MAC:\s*([0-9a-fA-F:]{17})", identity.stdout)
    if not match: raise ValueError("Cannot identify the connected board")
    mac = match[1].lower()
    backup = args.backup
    if not backup:
        directory = ROOT / "backups" / datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        directory.mkdir(parents=True, exist_ok=False)
        backup = directory / "flash-before.bin"
        tool("read_flash", "0", "0x1000000", backup)
        backup.with_suffix(".device.json").write_text(json.dumps({"mac": mac}), encoding="utf-8")
    else:
        metadata = json.loads(backup.with_suffix(".device.json").read_text())
        if metadata.get("mac") != mac: raise ValueError("The backup belongs to another board")
    original = backup.read_bytes()
    validate_backup(original)
    report = {"backup": str(backup.resolve()), "mac": mac, "sha256": sha(original), "protected": {
        name: {"offset": offset, "size": size, "sha256": sha(original[offset:offset+size])}
        for name, (offset, size) in DATA_PARTITIONS.items()}, "factory_sha256": sha(original[0x10000:0x110000])}
    backup.with_suffix(".report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("Migration plan: bootloader @0, partition table @0x8000, application A @0xc00000, boot selection @0x1f0000.")
    print("Factory firmware, patches, waves and samples are NOT written. Backup:", backup)
    if args.check_only:
        return
    # Prove that a reused backup belongs to the currently connected board/layout.
    snapshot = backup.with_suffix(".identity.bin")
    tool("read_flash", "0x8000", "0x1000", snapshot)
    if snapshot.read_bytes() != original[0x8000:0x9000]:
        raise ValueError("Device partition table differs from the backup")
    # Do not activate A until its image and the rollback-enabled bootloader are written and verified.
    tool("write_flash", "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB",
        "0x0", args.release / "bootloader.bin", "0x8000", args.release / "partition-table.bin",
        "0xc00000", args.release / kit["application"])
    # Read protected storage back BEFORE selecting A. A failure leaves the factory image bootable.
    checked = backup.with_suffix(".protected-after.bin")
    tool("read_flash", "0x9000", "0xbf7000", checked)
    after = checked.read_bytes()
    for name, (offset, size) in {**DATA_PARTITIONS, "factory": (0x10000, 0x100000)}.items():
        if after[offset-0x9000:offset-0x9000+size] != original[offset:offset+size]:
            raise ValueError("Protected data changed; A was not activated: " + name)
    tool("write_flash", "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB", "0x1f0000", args.release / "boot-a.bin")
    print("Migration verified; restart the Modulox. A must pass its boot self-test; otherwise the original factory firmware remains the fallback.")

if __name__ == "__main__":
    main()
