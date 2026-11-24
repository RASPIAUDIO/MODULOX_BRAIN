import { readFile } from "node:fs/promises";
import vm from "node:vm";
import { tr } from "../dist/i18n.mjs";
import assert from "node:assert/strict";
import { offlineCatalog, validatePatch, sharedAudioNodes, modulePaletteGroups, modulationError } from "../dist/patch.mjs";
import { packPatch, unpackPatch } from "../dist/patch-files.mjs";
import { estimateResources } from "../dist/resources.mjs";

const patch = { version: 3, name: "INPUT Reverb", settings: { Voices: 8 }, modules: [
  { id: "mic", type: "INPUT", parameters: { Level: 75 } },
  { id: "filter", type: "TB-303", parameters: {} },
  { id: "st", type: "Stereo", parameters: {} },
  { id: "rev", type: "Reverb", parameters: {} },
  { id: "lfo", type: "LFO", parameters: {} },
], connections: [["mic", "filter", 0], ["filter", "st", 0], ["st", "rev", 0], ["rev", "out", 0]],
  modulations: [["lfo", "filter", "Cutoff"]], midiLearn: [["mic", "Level", 7]] };
assert.equal(validatePatch(patch, offlineCatalog), "");
assert.equal(offlineCatalog.audioInput, true);
const spec = offlineCatalog.modules.find(node => node.type === "INPUT");
assert.equal(spec.inputs, 0); assert.equal(spec.outputChannels, 1); assert.equal(spec.sharedSource, true);
assert.equal(modulePaletteGroups(offlineCatalog).find(g => g.id === "generators").modules.includes(spec), true);
for (const id of ["mic", "filter", "st", "rev", "out"]) assert.ok(sharedAudioNodes(patch, offlineCatalog).has(id));
const restored = (await unpackPatch(await packPatch(patch, { waves: new Map(), samples: new Map() }))).patch;
assert.equal(validatePatch(restored, offlineCatalog), "");
assert.deepEqual(restored.modules, patch.modules); assert.deepEqual(restored.midiLearn, patch.midiLearn);
const without = structuredClone(offlineCatalog); without.modules = without.modules.filter(m => m.type !== "INPUT");
assert.notEqual(validatePatch(patch, without), "");
for (const level of [-1, 101, 0.5]) {
  const bad = structuredClone(patch); bad.modules[0].parameters.Level = level;
  assert.notEqual(validatePatch(bad, offlineCatalog), "");
}
const duplicate = structuredClone(patch); duplicate.modules.push({ id: "mic2", type: "INPUT", parameters: {} });
assert.notEqual(validatePatch(duplicate, offlineCatalog), "");
const gate = structuredClone(patch); gate.modules[1].type = "ADSR";
assert.notEqual(validatePatch(gate, offlineCatalog), "");
const mono = structuredClone(patch); mono.connections = [["mic", "rev", 0], ["rev", "out", 0]];
assert.notEqual(validatePatch(mono, offlineCatalog), "");
const cv = structuredClone(patch); cv.modules[4].type = "Env";
assert.notEqual(modulationError(cv, offlineCatalog, "lfo", "filter", "Cutoff"), "");
const calibration = { base: 1, modules: Object.fromEntries(patch.modules.map(m => [m.type, 1])) };
const one = estimateResources({ ...patch, settings: { Voices: 1 } }, offlineCatalog, { calibration });
const eight = estimateResources(patch, offlineCatalog, { calibration });
assert.equal(eight.cpu, one.cpu); // One stream and one effect chain, irrespective of Voices.
assert.equal(eight.rows.find(row => row.type === "INPUT").bytes, 0);
console.log("PASS: INPUT source/limits, mono/stereo routing, shared CV rules, MIDI Learn/portable round-trip, offline/old firmware and constant polyphony cost");

// Exercise the actual UI gate while keeping an offline draft editable on an older unit.
const source = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const nodes = new Map();
const draft = { key: "draft-input", patch };
const context = { midiKeyboard: { setBusy() {} }, deviceScreen: { update() {} }, busy: "", port: {},
  currentFirmware: null, deviceCatalog: { ...offlineCatalog, audioInput: false }, catalog: { ...offlineCatalog, patchManagement: true },
  current: () => draft, activeDraft: () => draft, renderFirmware() {}, renderResources() {}, renderSampleDetail() {},
  viewport: {}, canUpload: true, hasRead: true, tr, drafts: [draft], selection: { kind: "draft" },
  navigator: { serial: {} }, screen: "editor", editorView: "graph", draftStatus: () => "draft", selectedNode: undefined,
  resourceError: () => "", patchSampleError: () => "", factoryAbort: null, cableSource: null,
  $: id => { if (!nodes.has(id)) nodes.set(id, { setAttribute() {}, toggleAttribute() {} }); return nodes.get(id); } };
vm.runInNewContext(source.slice(source.indexOf("function renderActions()"), source.indexOf("function samplerUnavailable()")), context);
context.renderActions();
assert.equal(nodes.get("upload").disabled, true);
assert.match(nodes.get("upload").title, /INPUT/);
assert.equal(nodes.get("details").inert, false);
context.deviceCatalog.audioInput = true;
context.renderActions(); assert.equal(nodes.get("upload").disabled, false);
const example = JSON.parse(await readFile(new URL("../examples/INPUT-Reverb.json", import.meta.url), "utf8"));
assert.equal(validatePatch(example, offlineCatalog), "");
