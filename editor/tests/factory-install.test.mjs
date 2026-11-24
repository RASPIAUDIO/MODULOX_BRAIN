import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { factoryFlashBytes, validateFactoryRelease, validateFactoryImage, fetchFactoryRelease, fetchFactoryImage,
  factoryConsentValid, prepareFactoryInstaller } from "../dist/factory-install.mjs";

const root = new URL("../dist/factory/", import.meta.url);
const release = JSON.parse(await readFile(new URL("latest.json", root)));
const image = new Uint8Array(await readFile(new URL(release.file, root)));
assert.equal(image.length, factoryFlashBytes);
assert.equal(createHash("md5").update(image).digest("hex"), release.md5);
assert.equal(JSON.stringify(release).includes("74:4d:bd"), false);
await validateFactoryImage(image, release);
for (const change of [{ size: 676016 }, { model: "other" }, { file: "../image.bin" }, { psramBytes: 0x200000 }])
  assert.throws(() => validateFactoryRelease({ ...release, ...change }));
await assert.rejects(validateFactoryImage(image.subarray(0, image.length - 1), release), /16 Mio/);
const corrupt = image.slice(); corrupt[400] ^= 1;
await assert.rejects(validateFactoryImage(corrupt, release), /SHA-256/);
const wrongLayout = image.slice(); wrongLayout[0x8000 + 4] ^= 1;
await assert.rejects(validateFactoryImage(wrongLayout, { ...release, sha256: createHash("sha256").update(wrongLayout).digest("hex") }));
assert.equal(factoryConsentValid(true, "EFFACER"), true);
for (const [check, phrase] of [[false, "EFFACER"], [true, "effacer"], [true, ""], [true, "EFFACER TOUT"]])
  assert.equal(factoryConsentValid(check, phrase), false);
assert.deepEqual(await fetchFactoryRelease(async () => ({ ok: true, json: async () => release })), release);
await assert.rejects(fetchFactoryRelease(async () => ({ ok: false })), /Aucune image/);
await assert.rejects(fetchFactoryImage({ ...release, file: "../image.bin" }, () => { throw new Error("must not fetch"); }), /invalide/);

function connection(options = {}) {
  const calls = [];
  const loader = {
    chip: { CHIP_NAME: options.chip ?? "ESP32-S3",
      getChipFeatures: async () => [options.psram ?? "Embedded PSRAM 8MB (AP_3v3)"],
      readMac: async () => options.mac ?? "74:4d:bd:00:11:22" },
    main: async mode => { calls.push("identify"); assert.equal(mode, "no_reset"); },
    getSecurityInfo: async () => options.security ?? { flags: 0, chipId: 9, flashCryptCnt: 0 },
    detectFlashSize: async () => options.flash ?? "16MB",
    readReg: async address => address === 0x60007054 ? ((options.psram ? 2 : 1) << 3) | (1 << 7) : 0,
    writeFlash: async args => {
      calls.push("write");
      assert.equal(args.fileArray.length, 1); assert.equal(args.fileArray[0].address, 0);
      assert.equal(args.fileArray[0].data, image); assert.equal(args.fileArray[0].data.length, factoryFlashBytes);
      assert.equal(args.flashSize, "keep"); assert.equal(args.flashMode, "keep"); assert.equal(args.flashFreq, "keep");
      assert.equal(args.eraseAll, false); assert.equal(args.compress, true);
      if (options.writeError) throw new Error("USB disconnected");
      args.reportProgress(0, 50, 100); args.reportProgress(0, 100, 100);
    },
    flashMd5sum: async (address, length) => { calls.push("verify"); assert.equal(address, 0); assert.equal(length, factoryFlashBytes); return options.md5 ?? release.md5; },
    after: async () => { calls.push("reset"); if (options.resetError) throw new Error("USB reset"); },
  };
  const transport = { disconnect: async () => { calls.push("close"); } };
  return { calls, connect: async () => ({ loader, transport }) };
}
let connected = false;
await assert.rejects(prepareFactoryInstaller({}, corrupt, release, () => {}, async () => { connected = true; }), /SHA-256/);
assert.equal(connected, false);
for (const options of [{ chip: "ESP32-C3" }, { flash: "8MB" }, { psram: "Embedded PSRAM 2MB" }, { mac: "unknown" },
  { security: {} }, { security: { flags: 1, chipId: 9, flashCryptCnt: 0 } },
  { security: { flags: 4, chipId: 9, flashCryptCnt: 0 } }, { security: { flags: 0, chipId: 9, flashCryptCnt: 1 } }]) {
  const mock = connection(options);
  await assert.rejects(prepareFactoryInstaller({}, image, release, () => {}, mock.connect));
  assert.deepEqual(mock.calls, ["identify", "close"]);
}
const mock = connection(), progress = [];
const session = await prepareFactoryInstaller({}, image, release, (message, percent) => progress.push(percent), mock.connect);
assert.deepEqual(mock.calls, ["identify"]); // Preparing cannot erase, write, verify or reset.
assert.equal(session.device.mac, "74:4d:bd:00:11:22");
await assert.rejects(session.install({ checked: false, phrase: "EFFACER" }), /Coche/);
await assert.rejects(session.install({ checked: true, phrase: "effacer" }), /EFFACER/);
assert.deepEqual(mock.calls, ["identify"]);
assert.deepEqual(await session.install({ checked: true, phrase: "EFFACER" }), { version: release.firmwareVersion, mac: session.device.mac });
assert.deepEqual(mock.calls, ["identify", "write", "verify", "reset", "close"]);
assert.equal(progress.at(-1), 100);
await assert.rejects(session.install({ checked: true, phrase: "EFFACER" }), /nouveau/);

const cancelled = connection();
const cancelledSession = await prepareFactoryInstaller({}, image, release, () => {}, cancelled.connect);
await cancelledSession.close();
await assert.rejects(cancelledSession.install({ checked: true, phrase: "EFFACER" }));
assert.deepEqual(cancelled.calls, ["identify", "close"]);
const changedImage = image.slice(), changed = connection();
const changedSession = await prepareFactoryInstaller({}, changedImage, release, () => {}, changed.connect);
changedImage[500] ^= 1;
await assert.rejects(changedSession.install({ checked: true, phrase: "EFFACER" }), error => error.factoryWriteStarted === false && /SHA-256/.test(error.message));
assert.deepEqual(changed.calls, ["identify", "close"]);
for (const options of [{ writeError: true }, { md5: "0".repeat(32) }]) {
  const mock = connection(options);
  const session = await prepareFactoryInstaller({}, image, release, () => {}, mock.connect);
  await assert.rejects(session.install({ checked: true, phrase: "EFFACER" }), error => error.factoryWriteStarted === true);
  assert.equal(mock.calls.at(-1), "close"); assert.ok(!mock.calls.includes("reset"));
}
const resetFailure = connection({ resetError: true });
const verified = await prepareFactoryInstaller({}, image, release, () => {}, resetFailure.connect);
assert.equal((await verified.install({ checked: true, phrase: "EFFACER" })).version, release.firmwareVersion);
console.log("PASS: actual full image/boot/SHA/MD5, target/security checks, read-only preparation, explicit one-use consent, full 16 MiB write and verification, cancellation/errors/reset cleanup");
