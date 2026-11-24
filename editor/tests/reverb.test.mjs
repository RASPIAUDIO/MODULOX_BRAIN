import assert from "node:assert/strict";
import { offlineCatalog, validatePatch, sharedAudioNodes } from "../dist/patch.mjs";
import { packPatch, unpackPatch } from "../dist/patch-files.mjs";

const patch = {
  version: 3, name: "Reverb test",
  modules: [
    { id: "osc", type: "Osc", parameters: {} },
    { id: "amp", type: "ADSR", parameters: {} },
    { id: "st", type: "Stereo", parameters: {} },
    { id: "rev", type: "Reverb", parameters: { Decay: 3500, Damping: 55, "Pre-delay": 30, Width: 80, Mix: 35 } },
    { id: "chorus", type: "Chorus", parameters: {} },
    { id: "lfo", type: "LFO", parameters: {} },
  ],
  connections: [["osc", "amp", 0], ["amp", "st", 0], ["st", "rev", 0], ["rev", "chorus", 0], ["chorus", "out", 0]],
  modulations: [["lfo", "rev", "Mix"]],
  midiLearn: [["rev", "Decay", 74]],
};
assert.equal(validatePatch(patch, offlineCatalog), "");
assert.ok(sharedAudioNodes(patch, offlineCatalog).has("rev"));
assert.ok(sharedAudioNodes(patch, offlineCatalog).has("chorus"));
const file = await packPatch(patch, { waves: new Map(), samples: new Map() });
const restored = (await unpackPatch(file)).patch;
assert.equal(validatePatch(restored, offlineCatalog), "");
assert.deepEqual(restored.modules.find(n => n.id === "rev").parameters, patch.modules[3].parameters);
assert.deepEqual(restored.modulations, patch.modulations);
assert.deepEqual(restored.midiLearn, patch.midiLearn);
const mono = structuredClone(patch); mono.connections[2][0] = "amp";
assert.notEqual(validatePatch(mono, offlineCatalog), "");
const duplicate = structuredClone(patch); duplicate.modules.push({ id: "rev2", type: "Reverb", parameters: {} });
assert.notEqual(validatePatch(duplicate, offlineCatalog), "");
const legacy = structuredClone(offlineCatalog); legacy.modules = legacy.modules.filter(n => n.type !== "Reverb");
assert.notEqual(validatePatch(patch, legacy), "");
console.log("PASS: reverb stereo graph, shared effect, parameter/CV/MIDI round-trip, channel/instance/legacy rejection");
