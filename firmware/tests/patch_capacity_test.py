"""Check actual NVS allocation using Espressif's installed generator, without touching a device."""
from pathlib import Path
import csv
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "build" / "capacity-test"
DEST.mkdir(parents=True, exist_ok=True)
presets = [json.loads(file.read_text(encoding="utf-8")) for file in sorted((ROOT.parent / "editor/dist/presets").glob("*.json"))]
presets = [p for p in presets if isinstance(p, dict)]
assert len(presets) == 40
encode = lambda patch: json.dumps(patch, ensure_ascii=False, separators=(",", ":")).encode()
largest = max(presets, key=lambda p: len(encode(p)))

def generate(name, blobs, expect_success):
    source = DEST / (name + ".csv")
    with source.open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(["key", "type", "encoding", "value"])
        writer.writerow(["patches", "namespace", "", ""])
        for i, blob in enumerate(blobs):
            writer.writerow([f"u{i:013x}", "data", "hex2bin", blob.hex()])
    result = subprocess.run([sys.executable, "-m", "esp_idf_nvs_partition_gen", "generate", str(source),
        str(DEST / (name + ".bin")), "0x20000"], capture_output=True, text=True)
    if expect_success:
        assert result.returncode == 0, result.stdout + result.stderr
        assert (DEST / (name + ".bin")).stat().st_size == 0x20000
    else:
        assert result.returncode != 0, "64 maximum-size patches must exceed the existing partition"
    return {"patches": len(blobs), "jsonBytes": sum(map(len, blobs)), "fits128KiB": expect_success}

report = {
    "representative64": generate("representative64", [encode(presets[i % len(presets)]) for i in range(64)], True),
    "largestPreset64": generate("largest64", [encode(largest)] * 64, True),
    "maxSize64": generate("maxsize64", [b"x" * 4096] * 64, False),
}
(DEST / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report))
print("PASS: official NVS generator fits 64 representative patches and 64 copies of the largest preset; 64 maximum-size blobs exceed 128 KiB")
