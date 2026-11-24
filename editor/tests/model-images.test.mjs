import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { boardProfiles, boardAssetPath } from "../dist/hardware.mjs";
import { validateFirmware, fetchFirmwareRelease } from "../dist/firmware.mjs";
import { validateFactoryImage, prepareFactoryInstaller } from "../dist/factory-install.mjs";
import { recoveryPlan } from "../dist/firmware-recovery.mjs";
const assets = new URL("../dist/", import.meta.url);
const all = [];
for (const board of Object.values(boardProfiles)) {
  const folder = boardAssetPath("firmware", board).slice(1);
  const release = JSON.parse(await readFile(new URL(folder + "latest.json", assets)));
  const image = new Uint8Array(await readFile(new URL(folder + release.file, assets)));
  const factoryFolder = boardAssetPath("factory", board).slice(1);
  const factory = JSON.parse(await readFile(new URL(factoryFolder + "latest.json", assets)));
  const full = new Uint8Array(await readFile(new URL(factoryFolder + factory.file, assets)));
  const device = { model: board.model, layout: board.layout, protocol: 1, slotBytes: board.slotBytes, usbUpdate: true };
  await validateFirmware(image, release, device);
  await validateFactoryImage(full, factory);
  assert.equal(createHash("md5").update(full).digest("hex"), factory.md5);
  assert.equal(full[3] >>> 4, board.flashBytes === 0x1000000 ? 4 : 5);
  const plan = recoveryPlan(full.subarray(0x8000, 0x9000), full.subarray(0x1f0000, 0x1f2000), board);
  assert.ok([board.ota0, board.ota1].includes(plan.address));
  all.push({ board, release, image, factory, full, device });
}
for (const own of all) for (const other of all.filter(x => x !== own)) {
  await assert.rejects(validateFirmware(own.image, own.release, other.device), /Modulox/);
  await assert.rejects(validateFirmware(own.image, { ...own.release, model: other.board.model, layout: other.board.layout }), /autre appareil/);
  await assert.rejects(fetchFirmwareRelease(async () => ({ ok: true, json: async () => other.release }), own.board.model), /autre modèle/);
}

// Test the actual factory selection code with both shipped images. A ROM
// identification must finish before any image is requested or any write occurs.
const originalFetch = globalThis.fetch;
try {
  for (const own of all) {
    const calls = [], board = own.board;
    let identified = false;
    const loader = {
      chip: { CHIP_NAME: "ESP32-S3", readMac: async () => { identified = true; return "b4:3a:45:0c:33:7c"; } },
      main: async () => calls.push("connect"),
      getSecurityInfo: async () => ({ chipId: 9, flags: 0, flashCryptCnt: 0 }),
      detectFlashSize: async () => `${board.flashBytes / 1048576}MB`,
      readReg: async address => address === 0x60007054 ? ((board.psramBytes === 0x1000000 ? 3 : 1) << 3) | (board.psramVendor << 7)
        : address === 0x6000703c && board.octalFlash ? 1 << 9 : 0,
      writeFlash: async args => {
        calls.push("write"); assert.equal(args.fileArray[0].address, 0);
        assert.equal(args.fileArray[0].data.length, board.flashBytes);
        assert.equal(createHash("sha256").update(args.fileArray[0].data).digest("hex"), own.factory.sha256);
      },
      flashMd5sum: async (offset, length) => { assert.equal(offset, 0); assert.equal(length, board.flashBytes); calls.push("verify"); return own.factory.md5; },
      after: async () => calls.push("reset"),
    };
    const connect = async () => ({ loader, transport: { disconnect: async () => calls.push("close") } });
    globalThis.fetch = async path => {
      assert.ok(identified, "Must identify the target before selecting a factory image");
      assert.ok(path.startsWith(boardAssetPath("factory", board)), path);
      calls.push("fetch");
      return { ok: true, json: async () => own.factory, arrayBuffer: async () => own.full.buffer };
    };
    const other = all.find(x => x !== own);
    await assert.rejects(prepareFactoryInstaller({}, other.full, other.factory, () => {}, connect), /autre modèle/);
    assert.deepEqual(calls, ["connect", "close"]); calls.length = 0;
    const session = await prepareFactoryInstaller({}, undefined, undefined, () => {}, connect);
    assert.equal(session.device.model, board.model);
    assert.equal(session.release.model, board.model);
    assert.deepEqual(calls, ["connect", "fetch", "fetch"]);
    await session.install({ checked: true, phrase: "EFFACER" });
    assert.deepEqual(calls, ["connect", "fetch", "fetch", "write", "verify", "reset", "close"]);
    globalThis.fetch = async path => ({ ok: false });
    calls.length = 0;
    await assert.rejects(prepareFactoryInstaller({}, undefined, undefined, () => {}, connect), /Aucune image/);
    assert.deepEqual(calls, ["connect", "close"]); // Missing correct image never falls back to another model or writes.
  }
} finally { globalThis.fetch = originalFetch; }
console.log("PASS: both real firmware/factory images, cross-model rejection, read-only automatic selection, no fallback, full writes and MD5 at 16/32 MiB");
