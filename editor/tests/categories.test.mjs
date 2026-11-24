import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { patchCategories, normalizePatch, validatePatch, offlineCatalog } from "../dist/patch.mjs";
import { packPatch, unpackPatch } from "../dist/patch-files.mjs";
import { fetchPatches } from "../dist/protocol.mjs";
const base = new URL("../dist/", import.meta.url);
const example = JSON.parse(await readFile(new URL("../../firmware/patches/01-basic.json", import.meta.url)));
const patch = normalizePatch(example);
assert.equal(validatePatch(patch, offlineCatalog), "");
assert.equal(patch.category, undefined); // Legacy patches remain valid and default to User on device.
assert.equal(patchCategories[0][0], "user");
assert.equal(offlineCatalog.maxPatches, 64);
for (const [category] of patchCategories) {
  const value = { ...patch, category };
  assert.equal(validatePatch(value, offlineCatalog), "");
  const file = await packPatch(value, { waves: new Map(), samples: new Map() });
  assert.equal((await unpackPatch(file)).patch.category, category);
}
for (const category of [null, 2, {}, "", "other"]) {
  assert.match(validatePatch({ ...patch, category }, offlineCatalog), /Catégorie/);
  await assert.rejects(unpackPatch(JSON.stringify({ ...patch, category })), /Catégorie/);
}
const files = (await readdir(new URL("presets/", base))).filter(file => /^\d{2}-.*\.json$/.test(file)).sort();
const counts = {};
for (const file of files) {
  const preset = JSON.parse(await readFile(new URL("presets/" + file, base)));
  assert.ok(patchCategories.some(([id]) => id === preset.category), file);
  counts[preset.category] = (counts[preset.category] ?? 0) + 1;
}
assert.deepEqual(counts, { bass: 8, lead: 9, pad: 7, keys: 8, fx: 6, drums: 2 });
// The editor must read the entire advertised bank, including indices above 15/31.
let stream;
const encoder = new TextEncoder(), decoder = new TextDecoder();
const respond = text => stream.enqueue(encoder.encode(text));
const replyJson = value => { const text = JSON.stringify(value); respond(`OK ${encoder.encode(text).length}\n${text}`); };
const port = {
  readable: new ReadableStream({ start(controller) { stream = controller; } }),
  writable: new WritableStream({ write(bytes) {
    const command = decoder.decode(bytes).trim();
    if (command === "CATALOG") replyJson({ maxPatches: 64, maxBytes: 4096, patchCategories: true });
    else if (command === "LIST") respond("OK 64\n" + Array.from({ length: 64 }, (_, i) => `${i} u${i.toString(16).padStart(13, "0")}.json\n`).join(""));
    else if (/^GET \d+$/.test(command)) replyJson({ ...patch, name: `Patch ${command.slice(4)}`, category: "lead" });
    else throw new Error("Unexpected command " + command);
  } }),
};
const downloaded = await fetchPatches(port);
assert.equal(downloaded.entries.length, 64);
assert.equal(downloaded.entries[63].patch.name, "Patch 63");
assert.equal(downloaded.entries[63].patch.category, "lead");
assert.equal(port.readable.locked, false);
assert.equal(port.writable.locked, false);
console.log("PASS: category file round-trips, legacy/invalid patches, 40 presets classified, complete 64-patch USB bank download");
