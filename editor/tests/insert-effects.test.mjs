import assert from "node:assert/strict";
import { offlineCatalog, validatePatch, sharedAudioNodes, moduleCategories } from "../dist/patch.mjs";
import { estimateResources, cpuCalibrationFor, MiB } from "../dist/resources.mjs";
import { packPatch, unpackPatch } from "../dist/patch-files.mjs";
import { fetchPatches } from "../dist/protocol.mjs";

const patch = { version: 3, name: "Acid crushed", modules: [
  { id: "osc", type: "Osc", parameters: {} },
  { id: "tb", type: "TB-303", parameters: { Cutoff: 1200, Reso: 90, Drive: 6, Mix: 100 } },
  { id: "dec", type: "Decimator", parameters: { Rate: 11025, Mix: 65 } },
  { id: "bits", type: "Bitcrusher", parameters: { Bits: 7, Mix: 50 } },
  { id: "amp", type: "ADSR", parameters: {} }, { id: "st", type: "Stereo", parameters: {} },
  { id: "comp", type: "Compressor", parameters: { Threshold: -20, Ratio: 4, Attack: 10, Release: 100, Makeup: 3, Mix: 100 } },
  { id: "lfo", type: "LFO", parameters: {} }, { id: "env", type: "Env", parameters: {} },
], connections: [["osc", "tb", 0], ["tb", "dec", 0], ["dec", "bits", 0], ["bits", "amp", 0], ["amp", "st", 0], ["st", "comp", 0], ["comp", "out", 0]],
  modulations: [["lfo", "dec", "Rate"], ["env", "tb", "Reso"]],
  midiLearn: [["tb", "Cutoff", 74], ["comp", "Ratio", 75], ["bits", "Bits", 76]], settings: { Voices: 8 } };
assert.equal(validatePatch(patch, offlineCatalog), "");
assert.ok(sharedAudioNodes(patch, offlineCatalog).has("comp"));
assert.equal(sharedAudioNodes(patch, offlineCatalog).has("tb"), false);
const saved = (await unpackPatch(await packPatch(patch, { waves: new Map(), samples: new Map() }))).patch;
assert.equal(validatePatch(saved, offlineCatalog), "");
assert.deepEqual(saved.modules, patch.modules); assert.deepEqual(saved.modulations, patch.modulations); assert.deepEqual(saved.midiLearn, patch.midiLearn);
const category = moduleCategories.find(c => c.id === "effects");
for (const type of ["Decimator", "Bitcrusher", "Compressor", "TB-303"]) assert.ok(category.types.includes(type));
const invalidStereo = structuredClone(patch); invalidStereo.connections[5][0] = "amp";
assert.notEqual(validatePatch(invalidStereo, offlineCatalog), "");
const perVoiceCompression = structuredClone(patch); perVoiceCompression.modulations.push(["env", "comp", "Threshold"]);
assert.match(validatePatch(perVoiceCompression, offlineCatalog), /globale/);
for (const [id, name, value] of [["dec", "Rate", 44101], ["bits", "Bits", 1], ["tb", "Reso", 100], ["comp", "Ratio", 0]]) {
  const invalid = structuredClone(patch); invalid.modules.find(n => n.id === id).parameters[name] = value;
  assert.notEqual(validatePatch(invalid, offlineCatalog), "");
}
const unknown = estimateResources(patch, offlineCatalog, { totalBytes: 16 * MiB,
  calibration: cpuCalibrationFor("modulox-brain-s3-32m16m", "1.6.0") });
assert.equal(unknown.extra, 0); // Only fixed internal state, with the normal per-module safety margin.
assert.equal(unknown.cpuComplete, false);
for (const type of ["Decimator", "Bitcrusher", "Compressor", "TB-303"]) assert.ok(unknown.unknownCpu.includes(type));
const legacy = structuredClone(offlineCatalog); legacy.modules = legacy.modules.filter(spec => !category.types.includes(spec.type) || spec.type === "Filter" || spec.type === "Disto");
assert.notEqual(validatePatch(patch, legacy), "");
// Expanded catalogs must survive the wire protocol above its former 20 KiB cap.
const expanded = { ...offlineCatalog, padding: "x".repeat(22000 - JSON.stringify(offlineCatalog).length) };
const encoder = new TextEncoder(), decoder = new TextDecoder();
const body = JSON.stringify(expanded); assert.ok(body.length > 20480 && body.length < 24576);
let stream;
const port = {
  readable: new ReadableStream({ start(controller) { stream = controller; } }),
  writable: new WritableStream({ write(bytes) {
    const command = decoder.decode(bytes).trim();
    if (command === "CATALOG") stream.enqueue(encoder.encode(`OK ${encoder.encode(body).length}\n${body}`));
    else if (command === "LIST") stream.enqueue(encoder.encode("OK 0\n"));
    else throw new Error("Unexpected command " + command);
  } }),
};
assert.equal((await fetchPatches(port)).catalog.modules.length, offlineCatalog.modules.length);
console.log("PASS: four insert modules, categories, stereo/shared validation, CV/MIDI/portable round-trip, parameter bounds, RAM and uncalibrated CPU");
