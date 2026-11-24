"""Assemble the real host rasterizer's four LCD frames into a PNG (standard library only)."""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1]
width, height = 584, 504
pixels = bytearray(bytes((18, 20, 24)) * width * height)
for i, name in enumerate(["low", "resonant", "high", "band"]):
    header, size, maximum, data = (root / "build" / f"filter-{name}.ppm").read_bytes().split(b"\n", 3)
    assert (header, size, maximum, len(data)) == (b"P6", b"280 240", b"255", 280 * 240 * 3)
    x, y = 8 + i % 2 * 288, 8 + i // 2 * 248
    for row in range(240):
        offset = ((y + row) * width + x) * 3
        pixels[offset:offset + 280 * 3] = data[row * 280 * 3:(row + 1) * 280 * 3]
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
scanlines = b"".join(b"\0" + pixels[row * width * 3:(row + 1) * width * 3] for row in range(height))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b"")
target = root / "build/display-v1.2-preview.png"
target.write_bytes(png)
print(target)
