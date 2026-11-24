import assert from "node:assert/strict";
import { readFile, readdir, writeFile, mkdir } from "node:fs/promises";
import { validatePatch, normalizePatch, offlineCatalog, moduleSpec } from "../dist/patch.mjs";
import { loadPresetAssets } from "../dist/preset-assets.mjs";
import { waveId } from "../dist/waves.mjs";

const root = new URL("../dist/presets/", import.meta.url);
const files = (await readdir(root)).filter(f => /^[a-z0-9-]+\.json$/.test(f)).sort();
assert.equal(files.length, 40);
const patches = await Promise.all(files.map(async f => JSON.parse(await readFile(new URL(f, root), "utf8"))));
const names = new Set(), waves = new Map(), samples = new Map();
const request = async path => {
  try { const bytes = await readFile(new URL(path.replace("/presets/", ""), root));
    return { ok: true, json: async () => JSON.parse(bytes), arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) };
  } catch { return { ok: false }; }
};
const storage = { waves, samples, fetch: request, saveWaves: async values => { assert.ok(values.length); }, saveSample: async value => { assert.ok(value.pcm.length); } };
for (let i = 0; i < patches.length; i++) {
  const patch = patches[i];
  assert.equal(patch.settings.Volume, 85, files[i]);
  assert.equal(validatePatch(normalizePatch(patch), offlineCatalog), "", files[i]);
  assert.ok(Buffer.byteLength(JSON.stringify(patch)) <= 4096, files[i]);
  assert.ok(!names.has(patch.name)); names.add(patch.name);
  if (i < 17) assert.equal(patch.modules.filter(n => n.type === "Osc").length, 3);
  await loadPresetAssets(patch, storage);
}
assert.equal(waves.size, 4); assert.equal(samples.size, 3);
for (const resource of [...waves.values(), ...samples.values()]) assert.equal(await waveId(resource.pcm), resource.id);
// Cached assets require no network; missing/corrupt dependencies never reach storage.
for (const patch of patches) await loadPresetAssets(patch, { ...storage, fetch: async () => { throw new Error("Unexpected network"); } });
const wavePatch = patches.find(p => p.waves?.length);
await assert.rejects(loadPresetAssets(wavePatch, { ...storage, waves: new Map(), fetch: async () => ({ ok: false }) }), /indisponible/);
await assert.rejects(loadPresetAssets(wavePatch, { ...storage, waves: new Map(), fetch: async path => path.endsWith("index.json")
  ? request(path) : { ok: true, arrayBuffer: async () => new ArrayBuffer(2048) } }), /corrompue/);
await assert.rejects(loadPresetAssets(wavePatch, { ...storage, waves: new Map(), saveWaves: async () => { throw new Error("Storage full"); } }), /Storage full/);

// Generate a native fixture from the exact bank JSON, not a second hand-authored bank.
const slots = {
  Osc: ["oscillator", "oscillator2", "oscillator3"], ADSR: ["envelope"], Filter: ["filter"], Mix: ["mixer"],
  LFO: ["lfo", "lfo2", "lfo3"], Env: ["env1", "env2"], Disto: ["disto"], Delay: ["delay"],
  Granulizer: ["granulizer"], FM4: ["fm4"], Noise: ["noise"], Pan: ["pan", "pan2", "pan3"],
  Chorus: ["chorus"], Rotary: ["rotary"], "Ring Mod": ["ring_mod"], Wavefolder: ["wavefolder"],
  Clock: ["clock1", "clock2"], Random: ["random1", "random2"], Slew: ["slew"],
};
const fixture = [];
for (const [index, p] of patches.entries()) {
  const counts = {}, ids = new Map();
  for (const n of p.modules) { const count = counts[n.type] ?? 0; ids.set(n.id, slots[n.type][count]); counts[n.type] = count + 1; }
  const lines = [`run_preset(${JSON.stringify(p.name)}, ${index < 9 || p.name.includes("Kick") ? 36 : 60}, [](SynthVoice& s, PatchSettings& settings, AudioPlan& plan, Resources& resources) {`];
  for (const [name, value] of Object.entries(p.settings ?? {})) lines.push(`set(settings.controls(), ${JSON.stringify(name)}, ${value});`);
  for (const n of p.modules) {
    const slot = ids.get(n.id);
    lines.push(`plan.modules |= module_bit(${slot});`);
    for (const [name, value] of Object.entries(n.parameters ?? {})) lines.push(`set(controls(s, ${slot}), ${JSON.stringify(name)}, ${value});`);
    if (n.type === "Osc" && n.parameters.Wave >= 3)
      lines.push(`resources.wave(s, ${["oscillator", "oscillator2", "oscillator3"].indexOf(slot)}, ${JSON.stringify(p.waves[n.parameters.Wave - 3])});`);
    if (n.type === "Granulizer") lines.push(`resources.sample(s, ${JSON.stringify(n.sample)});`);
  }
  lines.push(`plan.oscillators = ${counts.Osc ?? 0}; plan.has_envelope = ${Boolean(counts.ADSR)}; plan.has_filter = ${Boolean(counts.Filter)};`);
  for (const [from, to, port] of p.connections) lines.push(to === "out" ? `plan.output = ${ids.get(from)};` : `plan.inputs[${ids.get(to)}][${port}] = ${ids.get(from)};`);
  const done = new Set();
  function visit(id) {
    if (done.has(id)) return; done.add(id);
    for (const [from, to] of p.connections) if (to === id) visit(from);
    if (id !== "out") lines.push(`plan.order[plan.count++] = ${ids.get(id)};`);
  }
  visit("out");
  for (const [from, to, parameter] of p.modulations ?? []) lines.push(`modulate(plan, ${ids.get(from)}, ${ids.get(to)}, ${JSON.stringify(parameter)});`);
  for (const [from, to, input, output] of p.routings ?? []) {
    lines.push(`plan.controls[${ids.get(to)}][${["Value", "Trigger", "Reset", "Run"].indexOf(input)}] = {${ids.get(from)}, ${["CV", "Trigger", "Reset", "Run"].indexOf(output) - (output === "CV" ? 0 : 1)}};`);
  }
  for (const n of p.modules.filter(n => ["Random", "Slew"].includes(n.type))) lines.push(`plan.control_order[plan.control_count++] = ${ids.get(n.id)};`);
  lines.push("});"); fixture.push(lines.join("\n"));
}
await mkdir(new URL("../../firmware/build/", import.meta.url), { recursive: true });
await writeFile(new URL("../../firmware/build/preset-bank.inc", import.meta.url), fixture.join("\n"));
console.log(`PASS: ${patches.length} valid patches, 17 triple-oscillator basses/leads, ${waves.size} waves, ${samples.size} samples, cache/error/integrity handling`);
