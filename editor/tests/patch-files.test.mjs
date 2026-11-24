import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { packPatch, unpackPatch, readPatchFile, writePatchFile, maxPatchFileBytes } from "../dist/patch-files.mjs";
import { validatePatch, offlineCatalog } from "../dist/patch.mjs";
import { waveId } from "../dist/waves.mjs";

const root = new URL("../dist/presets/", import.meta.url);
const manifest = JSON.parse(await readFile(new URL("assets/index.json", root)));
const libraries = { waves: new Map(), samples: new Map() };
for (const kind of ["waves", "samples"]) for (const item of manifest[kind]) {
  const pcm = new Uint8Array(await readFile(new URL(`assets/${item.id}.pcm`, root)));
  libraries[kind].set(item.id, { id: item.id, name: item.name, pcm });
}
const files = (await readdir(root)).filter(f => f !== "index.json" && f.endsWith(".json"));
let granularText, waveformText;
// Every factory patch can become a self-contained personal file, then be
// reopened on a machine with no libraries and no server connection.
for (const file of files) {
  const patch = JSON.parse(await readFile(new URL(file, root), "utf8"));
  const contents = await packPatch(patch, libraries);
  const imported = await unpackPatch(contents);
  assert.deepEqual(imported.patch, patch);
  for (const kind of ["waves", "samples"]) for (const asset of imported[kind]) {
    assert.deepEqual(asset.pcm, libraries[kind].get(asset.id).pcm);
    assert.equal(await waveId(asset.pcm), asset.id);
  }
  if (imported.samples.length) granularText = contents;
  if (imported.waves.length) waveformText = contents;
}
assert.ok(granularText && waveformText);
const changed = JSON.parse(granularText); changed.samples[0].pcm = changed.samples[0].pcm.replace(/^./, changed.samples[0].pcm[0] === "A" ? "B" : "A");
await assert.rejects(unpackPatch(JSON.stringify(changed)), /corrompue/);
const missing = JSON.parse(waveformText); missing.waves = [];
await assert.rejects(unpackPatch(JSON.stringify(missing)), /incomplètes/);
const duplicate = JSON.parse(waveformText); duplicate.waves.push(duplicate.waves[0]);
await assert.rejects(unpackPatch(JSON.stringify(duplicate)), /incomplètes/);
await assert.rejects(unpackPatch('{broken'), /JSON/);
await assert.rejects(unpackPatch('{"format":"modulox.patch","version":999}'), /incompatible/);
await assert.rejects(readPatchFile({ size: maxPatchFileBytes + 1, text() { throw new Error("Must not read oversized files"); } }), /volumineux/);
const malformed = JSON.parse(waveformText); malformed.patch.modules[0].parameters.Level = 999;
await assert.rejects(unpackPatch(JSON.stringify(malformed)), /invalide/);
// In-progress work can be saved on disk, but still cannot be sent to hardware.
const draft = { version: 3, name: "Work in progress", modules: [{ id: "osc", type: "Osc" }, { id: "sampler", type: "Sampler" }], connections: [] };
const draftFile = await packPatch(draft, libraries);
assert.deepEqual((await unpackPatch(draftFile)).patch, draft);
assert.notEqual(validatePatch(draft, offlineCatalog), "");
const basic = JSON.parse(await readFile(new URL("01-va-iron-bass.json", root), "utf8"));
assert.deepEqual((await unpackPatch(JSON.stringify(basic))).patch, basic);
await assert.rejects(unpackPatch(JSON.stringify(JSON.parse(granularText).patch)), /absente/);
// Mock the standard File System Access contract: saves are separate files,
// preserve unrelated entries, and do not commit a failed write.
const disk = new Map([["unrelated.txt", "keep me"]]);
const directory = {
  async getFileHandle(name, { create }) {
    assert.equal(create, true); if (!disk.has(name)) disk.set(name, "");
    return { async createWritable() {
      let pending; return { async write(value) { pending = value; }, async close() { disk.set(name, pending); }, async abort() {} };
    } };
  },
  async removeEntry(name) { disk.delete(name); },
};
const a = await writePatchFile(directory, granularText, "My / patch ?");
const b = await writePatchFile(directory, granularText, "My / patch ?");
assert.notEqual(a, b); assert.ok(a.endsWith(".modulox.json"));
assert.deepEqual((await unpackPatch(disk.get(a))).patch, JSON.parse(granularText).patch);
assert.equal(disk.get("unrelated.txt"), "keep me");
const before = disk.size;
const failing = { ...directory, async getFileHandle(name, options) { await directory.getFileHandle(name, options); return { async createWritable() { return { async write() { throw new Error("Disk full"); }, async abort() {} }; } }; } };
await assert.rejects(writePatchFile(failing, granularText, "Failed"), /Disk full/);
assert.equal(disk.size, before);
console.log(`PASS: ${files.length} portable file round-trips, PCM integrity, invalid/oversized files, draft preservation, distinct disk files, failed writes preserve existing data`);
