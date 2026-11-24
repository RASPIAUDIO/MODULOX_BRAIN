import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { compareVersions, validateRelease, validateFirmware, readFirmwareFile, fetchFirmwareImage } from "../dist/firmware.mjs";
import { fetchFirmwareInfo, uploadFirmware } from "../dist/protocol.mjs";
const root = new URL("../dist/firmware/", import.meta.url);
const release = JSON.parse(await readFile(new URL("latest.json", root)));
const image = new Uint8Array(await readFile(new URL(release.file, root)));
const device = { ...release, slotBytes: 0x180000, usbUpdate: true, pendingVerify: false };
assert.equal(compareVersions("1.10.0", "1.2.9"), 1);
assert.equal(compareVersions("2.0.0-rc.10", "2.0.0-rc.2"), 1);
assert.equal(compareVersions("2.0.0", "2.0.0-rc.2"), 1);
assert.throws(() => compareVersions("garbage", "1.0.0"));
assert.throws(() => validateRelease({ ...release, model: "other" }));
await validateFirmware(image, release, device);
await assert.rejects(validateFirmware(image, release, { ...device, slotBytes: 1024 }), /Modulox/);
await assert.rejects(validateFirmware(image, { ...release, sha256: "0".repeat(64) }), /SHA-256/);
const damaged = image.slice(); damaged[400] ^= 1;
await assert.rejects(validateFirmware(damaged, release), /SHA-256/);
await assert.rejects(validateFirmware(image.subarray(0, image.length - 1), release), /Taille/);
const bundleText = await readFile(new URL(`modulox-${release.version}.modulox-fw.json`, root), "utf8");
assert.deepEqual((await readFirmwareFile({ size: bundleText.length, text: async () => bundleText })).image, image);
await assert.rejects(readFirmwareFile({ size: 4 * 1024 * 1024 }), /volumineux/);
await assert.rejects(fetchFirmwareImage({ ...release, file: "../secrets.bin" }, device, () => { throw new Error("must not fetch"); }), /Chemin/);

function serialMock({ rejectChunk = false, lostWrite = false } = {}) {
  const commands = [], received = []; let controller, expecting = 0, started = false, committed = false;
  const encoder = new TextEncoder(), decoder = new TextDecoder();
  const respond = text => { const bytes = encoder.encode(text); controller.enqueue(bytes.subarray(0, 2)); controller.enqueue(bytes.subarray(2)); };
  const port = {
    readable: new ReadableStream({ start(c) { controller = c; } }),
    writable: new WritableStream({ write(bytes) {
      if (expecting) {
        if (lostWrite) throw new Error("USB unplugged");
        assert.equal(bytes.length, expecting); expecting = 0;
        if (rejectChunk) { respond("ERR firmware-invalid\n"); return; }
        received.push(bytes.slice()); respond("OK 0\n"); return;
      }
      const command = decoder.decode(bytes).trim(); commands.push(command);
      if (command === "FWINFO") { const json = JSON.stringify(device); respond(`OK ${encoder.encode(json).length}\n${json}`); }
      else if (command.startsWith("FWBEGIN ")) { assert.equal(Number(command.split(" ")[1]), image.length); assert.ok(command.endsWith(release.sha256)); started = true; respond("OK 0\n"); }
      else if (command.startsWith("FWCHUNK ")) {
        assert.ok(started); const [, offset, length] = command.split(" ");
        assert.equal(Number(offset), received.reduce((n, b) => n + b.length, 0)); expecting = Number(length); respond("READY\n");
      } else if (command === "FWEND") { assert.deepEqual(Buffer.concat(received), Buffer.from(image)); committed = true; respond("OK 0\n"); }
      else if (command === "FWABORT") { started = false; respond("OK 0\n"); }
      else throw new Error("Unknown command " + command);
    } }),
    get committed() { return committed; }, commands,
  };
  return port;
}
const port = serialMock();
assert.equal((await fetchFirmwareInfo(port)).version, release.version);
let last = 0;
await uploadFirmware(port, image, release.sha256, (done, total) => { assert.ok(done > last); last = done; assert.equal(total, image.length); });
assert.equal(port.committed, true); assert.equal(last, image.length);
assert.equal(port.readable.locked, false); assert.equal(port.writable.locked, false);
const rejected = serialMock({ rejectChunk: true });
await assert.rejects(uploadFirmware(rejected, image, release.sha256)); assert.ok(rejected.commands.includes("FWABORT")); assert.equal(rejected.committed, false);
const cancelled = serialMock(), abort = new AbortController();
await assert.rejects(uploadFirmware(cancelled, image, release.sha256, () => abort.abort(), abort.signal), { name: "AbortError" });
assert.ok(cancelled.commands.includes("FWABORT")); assert.ok(!cancelled.commands.includes("FWEND"));
const unplugged = serialMock({ lostWrite: true });
await assert.rejects(uploadFirmware(unplugged, image, release.sha256), /USB unplugged/); assert.equal(unplugged.committed, false);
assert.equal(unplugged.readable.locked, false); assert.equal(unplugged.writable.locked, false);
console.log(`PASS: firmware ${release.version}, binary/project/chip/build/SHA checks, offline file, semver, USB framing/chunks/ACKs, cancellation/rejection/unplug, locks released`);
