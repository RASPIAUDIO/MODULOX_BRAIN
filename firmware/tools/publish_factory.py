"""Publish a verified full-flash model image for the editor's factory installation."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import shutil
from release import ROOT, validate_layout

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Export directory containing manifest.json and the complete image")
    args = parser.parse_args()
    source = json.loads((args.source / "manifest.json").read_text(encoding="utf-8"))
    filename = source.get("file", "")
    if Path(filename).name != filename or not filename: raise ValueError("Invalid image filename")
    data = (args.source / filename).read_bytes()
    if (source.get("format") != "modulox.full-flash" or source.get("model") != "modulox-brain-s3"
            or source.get("layout") != "ab-1.5m-v1" or len(data) != 0x1000000
            or hashlib.sha256(data).hexdigest() != source.get("sha256")
            or source.get("source", {}).get("flashEncryption") is not False
            or source.get("source", {}).get("secureBoot") is not False
            or source.get("verification", {}).get("hardwareMd5Matched") is not True):
        raise ValueError("A verified, unencrypted full-flash Modulox export is required")
    parts = validate_layout(data[0x8000:0x9000])
    slot = source["boot"]["slot"]
    if slot not in ("ota_0", "ota_1") or not source["boot"].get("confirmed"): raise ValueError("Unconfirmed source firmware")
    offset = parts[slot]["offset"]
    version = data[offset + 48:offset + 80].split(b"\0")[0].decode("ascii")
    if (data[0] != 0xe9 or struct.unpack_from("<H", data, 12)[0] != 9 or
            struct.unpack_from("<I", data, offset + 32)[0] != 0xabcd5432 or version != source["boot"]["version"]):
        raise ValueError("Invalid source firmware descriptor")
    public = ROOT.parent / "editor/dist/factory"
    public.mkdir(parents=True, exist_ok=True)
    public_name = f"modulox-usine-{version}.bin"
    manifest = {"format": "modulox.full-flash", "version": 1, "model": "modulox-brain-s3", "chip": "esp32s3",
        "layout": "ab-1.5m-v1", "file": public_name, "offset": 0, "size": len(data), "flashBytes": len(data),
        "psramBytes": 0x800000, "sha256": hashlib.sha256(data).hexdigest(), "md5": hashlib.md5(data).hexdigest(),
        "firmwareVersion": version, "bootSlot": slot, "buildId": data[offset + 176:offset + 208].hex()}
    # Public metadata contains no source MAC, port, machine path or device report.
    shutil.copyfile(args.source / filename, public / public_name)
    (public / "latest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Factory image {version}: {len(data)} bytes, SHA-256 {manifest['sha256']}")

if __name__ == "__main__": main()
