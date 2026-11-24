import assert from "node:assert/strict";
import vm from "node:vm";
import { readFile } from "node:fs/promises";
import { packPatch, readPatchFile, writePatchFile, unpackPatch } from "../dist/patch-files.mjs";
import { sampleModuleTypes } from "../dist/patch.mjs";

const source = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const code = source.slice(source.indexOf("async function fileOperation("), source.indexOf("function showLocalPatches("));
const patch = JSON.parse(await readFile(new URL("../dist/presets/01-va-iron-bass.json", import.meta.url), "utf8"));
const disk = new Map(); let picks = 0, remembered = 0, opened, allowed = true;
const directory = { name: "Mes sons", async queryPermission() { return allowed ? "granted" : "denied"; }, async requestPermission() { return "denied"; },
  async getFileHandle(name) { return { async createWritable() { let pending; return { async write(text) { pending = text; }, async close() { disk.set(name, pending); }, async abort() {} }; } }; },
  async removeEntry(name) { disk.delete(name); },
  async *values() { for (const [name, text] of disk) yield { kind: "file", name, async getFile() { return { size: text.length, text: async () => text }; } }; yield { kind: "file", name: "unrelated.txt" }; },
};
const nodes = new Map();
const element = (tag, text = "") => ({ textContent: text, children: [], append(...children) { this.children.push(...children); }, replaceChildren(...children) { this.children = children; }, close() {}, });
const $ = id => { if (!nodes.has(id)) nodes.set(id, element()); return nodes.get(id); };
const context = { $, element, busy: "", renderActions() {}, showError(message) { $("error").textContent = message; },
  window: { async showDirectoryPicker(options) { assert.equal(options.mode, "readwrite"); picks++; return directory; } },
  folderReady: Promise.resolve(), folderPreference: async handle => { assert.equal(handle, directory); remembered++; },
  patchFolder: undefined, packPatch, readPatchFile, writePatchFile, sampleModuleTypes,
  waveLibraryReady: Promise.resolve(), samplesReady: Promise.resolve(), waveLibrary: new Map(), sampleLibrary: new Map(),
  saveWaves: async () => {}, saveSample: async () => {},
  fetch() { throw new Error("Personal patches must not use the server"); },
  fetchWave() { throw new Error("Unexpected USB request"); }, fetchSample() { throw new Error("Unexpected USB request"); },
  current: () => ({ patch }), structuredClone, screen: "editor", createDraft(item) { opened = item.patch; return true; },
  port: undefined,
};
vm.createContext(context); vm.runInContext(code, context);
await $("save-local").onclick(); await $("save-local").onclick();
assert.equal(picks, 1); assert.equal(remembered, 1); assert.equal(disk.size, 2);
for (const text of disk.values()) assert.deepEqual((await unpackPatch(text)).patch, patch);
await context.showFolderPatches();
assert.equal($("folder-list").children.length, 2);
await $("folder-list").children[0].children[0].onclick();
assert.deepEqual(opened, patch); assert.equal(context.busy, "");
allowed = false;
await context.showFolderPatches();
assert.match($("folder-list").children[0].textContent, /Actualiser/);
await $("save-local").onclick(); assert.equal(disk.size, 2); assert.match($("error").textContent, /Autorise/);
allowed = true;
context.window.showDirectoryPicker = async () => { throw Object.assign(new Error("Cancelled"), { name: "AbortError" }); };
await $("choose-folder").onclick(); assert.equal($("error").textContent, ""); assert.equal(disk.size, 2);
context.window.showDirectoryPicker = undefined;
await $("choose-folder").onclick(); assert.match($("error").textContent, /Chrome ou Edge/);
console.log("PASS: folder picker/remembered handle, two independent saves, file reopening, denied/revoked permissions, cancellation, unsupported browser, zero personal HTTP requests");
