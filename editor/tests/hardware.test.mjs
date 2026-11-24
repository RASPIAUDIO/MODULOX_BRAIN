import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { boardProfiles, boardForModel, boardAssetPath, identifyBoard } from "../dist/hardware.mjs";
const source = JSON.parse(await readFile(new URL("../../firmware/boards/profiles.json", import.meta.url)));
for (const [id, board] of Object.entries(source)) assert.deepEqual(boardProfiles[id], { ...board, id });
const [oldBoard, newBoard] = Object.values(boardProfiles);
assert.equal(boardAssetPath("factory", oldBoard), "/factory/");
assert.equal(boardAssetPath("factory", newBoard), "/factory/s3-32m16m/");
assert.throws(() => boardForModel("unknown"));
function loader({ flash = "32MB", cap = 3, vendor = 2, octal = true, chip = "ESP32-S3" } = {}) {
  const registers = new Map([[0x60007054, ((cap & 3) << 3) | (vendor << 7)],
    [0x60007058, (cap >>> 2) << 19], [0x6000703c, octal ? 1 << 9 : 0]]);
  return { chip: { CHIP_NAME: chip }, detectFlashSize: async () => flash, readReg: async address => {
    assert.ok(registers.has(address), "Only documented identification fields may be read"); return registers.get(address);
  }, writeFlash() { assert.fail("Identification cannot write flash"); } };
}
assert.equal(await identifyBoard(loader()), newBoard);
assert.equal(await identifyBoard(loader({ flash: "16MB", cap: 1, vendor: 1, octal: false })), oldBoard);
for (const changes of [{ flash: "16MB" }, { flash: "64MB" }, { octal: false }, { vendor: 1 }, { cap: 1 },
  { cap: 7 }, { cap: 4 }, { chip: "ESP32-C3" }]) await assert.rejects(identifyBoard(loader(changes)));
console.log("PASS: shared board registry, both physical memory profiles, high PSRAM capacity bit, flash bus/voltage mismatch rejection, no writes");
