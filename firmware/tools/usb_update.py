"""Developer CLI for the same update protocol used by Chrome; no ROM flashing."""
import argparse
import hashlib
import json
import time
from pathlib import Path
import serial
from serial.tools import list_ports

class Modulox:
    def __init__(self, port):
        self.serial = serial.Serial(port, 115200, timeout=30, write_timeout=30)
        self.serial.dtr = True
    def close(self): self.serial.close()
    def command(self, text):
        self.serial.write((text + "\n").encode("ascii")); self.serial.flush()
        reply = self.serial.readline(256).decode("utf-8").strip()
        if not reply: raise TimeoutError("No USB reply")
        if reply.startswith("ERR"): raise RuntimeError(reply)
        return reply
    def info(self):
        reply = self.command("FWINFO")
        size = int(reply.removeprefix("OK "))
        data = self.serial.read(size)
        if len(data) != size: raise TimeoutError("Incomplete firmware info")
        return json.loads(data)
    def upload(self, image, bad_hash=False, cancel=False):
        digest = "0" * 64 if bad_hash else hashlib.sha256(image).hexdigest()
        assert self.command(f"FWBEGIN {len(image)} {digest}") == "OK 0"
        for offset in range(0, len(image), 4096):
            chunk = image[offset:offset+4096]
            assert self.command(f"FWCHUNK {offset} {len(chunk)}") == "READY"
            self.serial.write(chunk); self.serial.flush()
            reply = self.serial.readline(256).decode().strip()
            if reply != "OK 0": raise RuntimeError(reply)
            if cancel:
                assert self.command("FWABORT") == "OK 0"
                return
        assert self.command("FWEND") == "OK 0"

def application_ports():
    return [p.device for p in list_ports.comports() if p.vid == 0x303a and p.pid == 0x4009]

def wait_ready(timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for port in application_ports():
            connection = None
            ready = False
            try:
                connection = Modulox(port)
                info = connection.info()
                if not info["pendingVerify"]:
                    ready = True
                    return connection, info
            except (OSError, RuntimeError, ValueError, serial.SerialException): pass
            finally:
                if connection and not ready: connection.close()
        time.sleep(0.5)
    raise TimeoutError("No confirmed Modulox firmware detected")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--image", type=Path)
    parser.add_argument("--hardware-test", action="store_true")
    parser.add_argument("--rollback-test-image", type=Path)
    args = parser.parse_args()
    if args.hardware_test:
        device, before = wait_ready()
        try:
            image = args.image.read_bytes()
            device.upload(image, cancel=True)
            assert device.info()["running"] == before["running"]
            try: device.upload(image, bad_hash=True)
            except RuntimeError as error: assert "firmware-hash" in str(error)
            else: raise AssertionError("Bad SHA was accepted")
            assert device.info()["running"] == before["running"]
            print("PASS: abort and bad SHA keep the active slot", flush=True)
            device.upload(image)
        finally: device.close()
        time.sleep(2)
        device, after = wait_ready()
        assert after["running"] != before["running"]
        print("PASS: USB update booted and confirmed", after["version"], after["running"], flush=True)
        assert device.command("FWROLLBACK") == "OK 0"
        device.close(); time.sleep(2)
        device, manual = wait_ready()
        assert manual["running"] == before["running"] and manual["buildId"] == before["buildId"]
        print("PASS: explicit USB rollback returned to the previous confirmed image", flush=True)
        device.upload(image); device.close(); time.sleep(2)
        device, after = wait_ready()
        assert after["buildId"] == image[176:208].hex()
        if args.rollback_test_image:
            try: device.upload(args.rollback_test_image.read_bytes())
            finally: device.close()
            time.sleep(12)
            device, rolled_back = wait_ready()
            assert rolled_back["running"] == after["running"] and rolled_back["buildId"] == after["buildId"]
            print("PASS: deliberately unconfirmed firmware rolled back to the previous valid image", flush=True)
            device.upload(image); device.close(); time.sleep(2)
            device, final = wait_ready()
            assert final["buildId"] == image[176:208].hex() and not final["pendingVerify"]
            print("PASS: test image replaced; production firmware confirmed", final["running"], flush=True)
        device.close()
        return
    device = Modulox(args.port) if args.port else wait_ready()[0]
    try:
        print(json.dumps(device.info(), indent=2))
        if args.image: device.upload(args.image.read_bytes()); print("Transfer verified; device will reboot.")
    finally: device.close()

if __name__ == "__main__": main()
