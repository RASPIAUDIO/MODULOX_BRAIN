"""Program an exported full-flash Modulox image. Default mode checks only."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys

FLASH_BYTES = 0x1000000

def validate_image(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if (manifest.get("format") != "modulox.full-flash" or manifest.get("chip") != "esp32s3"
            or manifest.get("flashBytes") != FLASH_BYTES or manifest.get("offset") != 0):
        raise ValueError("Manifest incompatible avec une image complete ESP32-S3 16 Mio")
    filename = manifest.get("file", "")
    if not filename or Path(filename).name != filename:
        raise ValueError("Nom de fichier image invalide")
    image_path = manifest_path.parent / filename
    image = image_path.read_bytes()
    if len(image) != FLASH_BYTES or manifest.get("size") != len(image):
        raise ValueError("Image incomplete : 16 777 216 octets requis")
    if hashlib.sha256(image).hexdigest() != manifest.get("sha256"):
        raise ValueError("SHA-256 incorrect : image modifiee ou corrompue")
    if image[0] != 0xe9 or struct.unpack_from("<H", image, 12)[0] != 9:
        raise ValueError("Le bootloader n'est pas une image ESP32-S3")
    return manifest, image_path

def run_esptool(port, baud, *command):
    args = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
            "--baud", str(baud), "--before", "no_reset", "--after", "no_reset", *map(str, command)]
    if command[0] in ("write_flash", "verify_flash"):
        subprocess.run(args, check=True)  # Keep programming progress visible in the console.
        return ""
    result = subprocess.run(args, check=True, capture_output=True, text=True)
    print(result.stdout, end="", flush=True)
    if result.stderr: print(result.stderr, file=sys.stderr, end="", flush=True)
    return result.stdout

def program(manifest_path, port, baud=921600, write=False, run=run_esptool):
    manifest, image = validate_image(manifest_path)
    print("Image verifiee :", image.name, "SHA-256", manifest["sha256"], flush=True)
    security = run(port, baud, "get_security_info")
    if not re.search(r"Secure Boot:\s*Disabled", security) or not re.search(r"Flash Encryption:\s*Disabled", security):
        raise ValueError("Cette copie brute necessite Secure Boot et chiffrement flash desactives sur la cible")
    identity = run(port, baud, "flash_id")
    if not re.search(r"Detected flash size:\s*16MB\b", identity):
        raise ValueError("La cible doit disposer de 16 Mio de flash")
    if not re.search(r"Embedded PSRAM 8MB\b", identity):
        raise ValueError("Ce kit correspond aux cartes ESP32-S3 avec PSRAM embarquee de 8 Mio")
    if not write:
        print("CONTROLE OK : aucune ecriture. Ajouter --write pour programmer cette carte.", flush=True)
        return
    print("Programmation de toute la flash a 0x000000 ; les donnees precedentes de cette cible seront remplacees.", flush=True)
    run(port, baud, "write_flash", "--flash_mode", "keep", "--flash_freq", "keep", "--flash_size", "keep", "0x0", image)
    run(port, baud, "verify_flash", "0x0", image)
    print("FLASH ET VERIFICATION OK. Appuyer sur RESET sans BOOT, puis controler ecran et audio.", flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("manifest.json"))
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=921600)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--write", action="store_true", help="Write and verify all 16 MiB at offset zero")
    mode.add_argument("--check-only", action="store_true", help="Inspect image and target without writing (default)")
    args = parser.parse_args()
    program(args.manifest, args.port, args.baud, args.write)

if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print("ECHEC :", error, file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stdout or "", error.stderr or "", file=sys.stderr)
        sys.exit(1)
