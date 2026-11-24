import assert from "node:assert/strict";
import { readFile, readdir } from "node:fs/promises";
import { offlineCatalog } from "../dist/patch.mjs";
import { loadFactoryBank, planFactoryUpload, uploadFactoryBank } from "../dist/factory-bank.mjs";

const root = new URL("../dist/presets/", import.meta.url);
const files = (await readdir(root)).filter(file => /^\d{2}-.*\.json$/.test(file)).sort();
let active = 0, peak = 0;
const request = async path => {
  ++active; peak = Math.max(peak, active);
  try {
    const bytes = path === "/presets/index.json" ? JSON.stringify(files) : await readFile(new URL(path.replace("/presets/", ""), root));
    return { ok: true, json: async () => JSON.parse(bytes) };
  } finally { --active; }
};
const items = await loadFactoryBank(request);
assert.equal(items.length, 40); assert.ok(peak <= 4);
assert.ok(items.every(item => item.patch.settings.Volume === 85));
await assert.rejects(loadFactoryBank(async () => ({ ok: true, json: async () => ["../bad.json"] })), /invalide/);
await assert.rejects(loadFactoryBank(async () => ({ ok: true, json: async () => [files[0], files[0]] })), /invalide/);
const catalog = { ...offlineCatalog, sampleNames: true };
const emptyPlan = await planFactoryUpload(items, [], catalog);
assert.equal(emptyPlan.added, 40);
assert.equal(new Set(emptyPlan.items.map(item => item.key)).size, 40);
assert.ok(emptyPlan.items.every(item => /^u[0-9a-f]{13}$/.test(item.key)));
assert.deepEqual((await planFactoryUpload(items, [], catalog)).items.map(item => item.key), emptyPlan.items.map(item => item.key));
await assert.rejects(planFactoryUpload(items, [], { ...catalog, maxPatches: 16 }), /Place insuffisante/);
await assert.rejects(planFactoryUpload(items, [], { ...catalog, maxBytes: 100 }), /taille/);
await assert.rejects(planFactoryUpload(items, [], { ...catalog, firmware: { pendingVerify: true } }), /confirmation/);

const legacy = items.slice(0, 10).map((item, i) => {
  const patch = structuredClone(item.patch); patch.settings.Volume = 65; delete patch.category;
  patch.layout = { out: { x: 11, y: 1234 } };
  return { id: i, file: `u${i.toString(16).padStart(13, "0")}.json`, patch };
});
const personal = structuredClone(legacy[0]); personal.file = "uabcabcabcabca.json";
personal.patch.modules[0].parameters.Level = 3;
const original = JSON.stringify(personal);
const oldEntries = [personal, ...legacy];
const adopted = await planFactoryUpload(items, oldEntries, catalog);
assert.equal(adopted.added, 30); assert.equal(adopted.updated, 10);
assert.equal(adopted.items[0].targetFile, legacy[0].file);
assert.ok(!adopted.items.some(item => item.targetFile === personal.file));
assert.equal(JSON.stringify(personal), original);
await assert.rejects(planFactoryUpload(items, Array.from({ length: 25 }, (_, i) => ({
  file: `u${(100 + i).toString(16).padStart(13, "0")}.json`, patch: personal.patch,
})), catalog), /Place insuffisante/);

const entries = [...oldEntries], calls = [], dependencies = [];
const options = {
  port: {}, catalog, libraries: { waves: new Map(), samples: new Map() },
  loadAssets: async patch => dependencies.push(patch.name),
  upload: async (_port, key, patch) => {
    assert.equal(dependencies.length, 40); // Every asset checked before any USB patch write.
    calls.push(key); assert.equal(patch.settings.Volume, 85);
    return entries.find(entry => entry.file === key + ".json")?.id ?? entries.length;
  },
  onSaved(entry) { const at = entries.findIndex(previous => previous.file === entry.file); if (at < 0) entries.push(entry); else entries[at] = entry; },
};
assert.deepEqual(await uploadFactoryBank(adopted, options), { saved: 40, done: 40, cancelled: false });
assert.equal(calls.length, 40);
assert.equal(JSON.stringify(entries.find(entry => entry.file === personal.file)), original);
const repeated = await planFactoryUpload(items, entries, catalog);
assert.equal(repeated.unchanged, 40); assert.equal(repeated.added, 0); assert.equal(repeated.updated, 0);
await uploadFactoryBank(repeated, { ...options, upload: async () => { throw new Error("must not resend"); } });
const customized = structuredClone(entries);
const first = customized.find(entry => entry.file === adopted.items[0].targetFile);
first.patch.modules[0].parameters.Level = 1;
const protectedCopy = await planFactoryUpload(items, customized, catalog);
assert.equal(protectedCopy.added, 1); assert.equal(protectedCopy.unchanged, 39);
assert.notEqual(protectedCopy.items[0].targetFile, first.file);
// Also protect customized slots originally created with a stable factory key.
const managed = emptyPlan.items.map((item, id) => ({ id, file: item.targetFile, patch: structuredClone(item.patch) }));
managed[0].patch.modules[0].parameters.Level = 1;
const managedPlan = await planFactoryUpload(items, managed, catalog);
assert.equal(managedPlan.added, 1); assert.notEqual(managedPlan.items[0].targetFile, managed[0].file);

let writes = 0;
await assert.rejects(uploadFactoryBank(emptyPlan, { ...options,
  loadAssets: async patch => { if (patch === emptyPlan.items[9].patch) throw new Error("asset missing"); },
  upload: async () => { ++writes; },
}), /asset missing/);
assert.equal(writes, 0);
const controller = new AbortController();
const cancelResult = await uploadFactoryBank(emptyPlan, { ...options, onSaved() {}, signal: controller.signal,
  loadAssets: async () => {}, upload: async () => { controller.abort(); return 0; },
});
assert.deepEqual(cancelResult, { saved: 1, done: 1, cancelled: true });
let saved = [];
await assert.rejects(uploadFactoryBank(emptyPlan, { ...options, loadAssets: async () => {}, onSaved: entry => saved.push(entry),
  upload: async () => { if (saved.length === 3) throw new Error("USB unplugged"); return saved.length; },
}), /3 patch.*USB unplugged/);
const resumed = await planFactoryUpload(items, saved, catalog);
assert.equal(resumed.unchanged, 3); assert.equal(resumed.added, 37);
console.log("PASS: 40 presets at 85%, bounded loading, capacity/firmware checks, unmodified legacy adoption, personal edits preserved, idempotence, asset failure before USB, cancellation and partial-transfer resume");
