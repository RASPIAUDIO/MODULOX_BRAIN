import assert from "node:assert/strict";
import vm from "node:vm";
import { readFile } from "node:fs/promises";
import { cpuCalibrationFor, estimateResources, validResourceSnapshot, validCpuValue, effectMemory, KiB, MiB } from "../dist/resources.mjs";
import { offlineCatalog as catalog } from "../dist/patch.mjs";
import { fetchResources, fetchFirmwareInfo } from "../dist/protocol.mjs";

const patch = { modules: [], connections: [], settings: { Voices: 1 } };
const snapshot = { version: 1, psramTotal: 8 * MiB, psramFree: 6 * MiB, psramLargest: 4 * MiB,
  internalFree: 60 * KiB, internalLargest: 40 * KiB, allocatedEffects: {}, cpuAverage: 25.4,
  cpuPeak: 47.2, overBudgetBlocks: 0, cpuValid: true };
assert.ok(validResourceSnapshot(snapshot));
for (const invalid of [{ ...snapshot, psramFree: 9 * MiB }, { ...snapshot, psramLargest: 7 * MiB },
  { ...snapshot, cpuAverage: NaN }, { ...snapshot, allocatedEffects: { Reverb: -1 } }])
  assert.equal(validResourceSnapshot(invalid), false);
const nodes = types => ({ ...patch, modules: types.map((type, i) => ({ id: "n" + i, type })) });
const estimate = (p, options = {}) => estimateResources(p, catalog, { snapshot, ...options });
let budget = estimate(nodes(["Reverb"]));
assert.equal(budget.extra, 99792);
assert.equal(budget.margin, KiB + Math.ceil(99792 * 0.1));
assert.equal(budget.remaining, snapshot.psramFree - budget.extra - budget.margin - 256 * KiB);
assert.equal(budget.cpuComplete, false); // No guessed CPU costs.
const retained = estimate(nodes(["Delay", "Reverb"]), { snapshot: { ...snapshot,
  allocatedEffects: { Delay: effectMemory.Delay.bytes, Reverb: effectMemory.Reverb.bytes } } });
assert.equal(retained.extra, 0); assert.equal(retained.margin, 2 * KiB);
const partial = estimate(nodes(["Delay"]), { snapshot: { ...snapshot, allocatedEffects: { Delay: 176400 } } });
assert.equal(partial.extra, 176400);
const samples = new Map([["a", { bytes: MiB }], ["b", { pcm: new Uint8Array(MiB) }]]);
const shared = nodes(["Sampler", "Sampler", "Granulizer"]);
shared.modules.forEach(n => n.sample = "a");
budget = estimate(shared, { samples }); assert.equal(budget.sampleBytes, MiB);
shared.modules[1].sample = "b";
budget = estimate(shared, { samples }); assert.equal(budget.sampleBytes, 2 * MiB);
assert.equal(estimate(nodes(["Sampler"])).sampleBytes, 0); // Empty sampler can receive a small sample even on a nearly full device.
const missing = nodes(["Sampler"]); missing.modules[0].sample = "missing";
assert.equal(estimate(missing).sampleBytes, MiB); // Chosen sample with missing metadata reserves max.
const tight = { ...snapshot, psramFree: 2 * MiB, psramLargest: 2 * MiB };
assert.ok(estimate(shared, { samples, snapshot: tight }).blocked);
assert.equal(estimate(shared, { samples, snapshot: { ...tight, psramTotal: 16 * MiB, psramFree: 10 * MiB, psramLargest: 8 * MiB } }).blocked, false);
assert.match(estimate(shared, { samples, snapshot: { ...snapshot, psramLargest: 400 * KiB } }).errors.join(), /fragmentée/);
assert.equal(estimate(shared, { samples, snapshot: { ...tight, allocatedEffects: { Delay: 352800 } } }).free, tight.psramFree); // Never reclaim old samples to hide the staging peak.
assert.equal(estimate(nodes(["Osc"]), { snapshot: undefined, totalBytes: 16 * MiB }).measured, false);

const cpuPatch = nodes(["Osc", "Reverb"]); cpuPatch.settings = { Voices: 8 };
const calibration = { base: 1, modules: { Osc: 2, Reverb: 10 } };
budget = estimate(cpuPatch, { calibration });
assert.equal(budget.cpuComplete, true);
assert.ok(Math.abs(budget.cpu - (1 + (8 * 2 + 10) * 1.1)) < 1e-9); // Shared Reverb once.
cpuPatch.modules[0].parameters = { Unison: 4 };
assert.ok(estimate(cpuPatch, { calibration }).blocked); // Voices and Unison use worst-case copies.
const monoCpu = structuredClone(cpuPatch); monoCpu.settings["Play mode"] = 1;
assert.equal(estimate(monoCpu, { calibration }).blocked, false);
const incomplete = estimate(cpuPatch, { calibration: { base: 1, modules: { Osc: 2 } } });
assert.ok(incomplete.blocked); assert.equal(incomplete.cpuComplete, false); // Known lower bound already exceeds CPU limit.
assert.equal(estimate(nodes(["Osc"]), { calibration: { base: 0, modules: { Osc: 0 } } }).cpuComplete, true); // Explicit measured zero is valid.

// User evidence is applied only to 16 MiB / 1.5.0, with a voice curve and
// a total Filter allowance rather than an eightfold charge at eight voices.
const model16 = "modulox-brain-s3-32m16m";
const reference = cpuCalibrationFor(model16, "1.5.0");
assert.deepEqual(reference.observations, { notes: [0, 1, 8], dry: [1.7, 10, 40], reverb: [37, 40, 65] });
assert.equal(cpuCalibrationFor("modulox-brain-s3", "1.5.0").base, undefined);
assert.equal(cpuCalibrationFor(model16, "1.5.1").base, undefined);
assert.equal(cpuCalibrationFor(model16, "1.6.0").referenceFirmware, "1.5.0");
assert.match(cpuCalibrationFor(model16, "1.6.0").note, /Extrapolation provisoire/);
assert.equal(cpuCalibrationFor("modulox-brain-s3", "1.6.0").base, undefined);
for (const [voices, expected] of [[1, 10.83], [8, 43.83]]) {
  const p = nodes(["Osc"]); p.settings = { Voices: voices };
  const result = estimate(p, { calibration: reference });
  assert.ok(Math.abs(result.cpu - expected) < 1e-8); assert.equal(result.cpuComplete, true);
}
const measuredPatch = nodes(["Osc", "Filter", "Reverb"]); measuredPatch.settings = { Voices: 8 };
const referenceResult = estimate(measuredPatch, { calibration: reference });
assert.ok(Math.abs(referenceResult.cpu - 84.53) < 1e-8);
assert.ok(Math.abs(referenceResult.rows.find(row => row.type === "Filter").cpu - 1.1) < 1e-8);
assert.equal(referenceResult.blocked, false);
measuredPatch.modules[0].parameters = { Unison: 4 };
assert.equal(estimate(measuredPatch, { calibration: reference }).blocked, true);
assert.equal(estimate(nodes(["Sampler"]), { calibration: reference }).cpuComplete, false);
const manual = cpuCalibrationFor(model16, "1.5.0", { base: 0, modules: { Osc: 0, Filter: 2 } });
assert.equal(estimate(nodes(["Osc"]), { calibration: manual }).cpu, 0);
assert.deepEqual(manual.totalCostModules, []); // Preserve old per-voice overrides.
const cleared = cpuCalibrationFor(model16, "1.5.0", { modules: { Reverb: null } });
assert.equal(estimate(nodes(["Reverb"]), { calibration: cleared }).cpuComplete, false);

// Actual add path, including a drag/drop caller, cannot bypass the RAM refusal.
const source = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const profileContext = { cpuCalibrationFor, currentFirmware: { model: model16, version: "1.5.0" },
  cpuCalibrations: new Map(), localStorage: { getItem() { return null; } } };
vm.runInNewContext(source.slice(source.indexOf("function cpuCalibrationKey()"), source.indexOf("function resourceBudget(")), profileContext);
assert.equal(profileContext.cpuCalibration().modules.Reverb, 36);
profileContext.currentFirmware.model = "modulox-brain-s3";
assert.equal(profileContext.cpuCalibration().modules.Reverb, undefined);
profileContext.currentFirmware = undefined; profileContext.resourceBoard = () => ({ model: model16 });
assert.equal(profileContext.cpuCalibration().modules.Reverb, undefined); // Unknown offline firmware is not presumed 1.5.0.
const fields = [{ dataset: { cpuType: "$base" }, value: "1.7" },
  { dataset: { cpuType: "Osc" }, value: "" }, { dataset: { cpuType: "Filter" }, value: "1" },
  { dataset: { cpuType: "Reverb" }, value: "36" }];
const ui = new Map(), storage = new Map();
const saveContext = { cpuCalibration: () => reference, validCpuValue, cpuCalibrationKey: () => "tested-profile",
  cpuCalibrations: new Map(), localStorage: { setItem(key, value) { storage.set(key, value); } },
  $: id => { if (!ui.has(id)) ui.set(id, { querySelectorAll: () => fields }); return ui.get(id); },
  renderActions() {}, screen: "patches" };
vm.runInNewContext(source.slice(source.indexOf('$("cpu-calibration-save").onclick'), source.indexOf("setInterval(() =>")), saveContext);
saveContext.$("cpu-calibration-save").onclick();
const savedReference = cpuCalibrationFor(model16, "1.5.0", JSON.parse(storage.get("tested-profile")));
assert.equal(estimate(nodes(["Osc"]), { calibration: savedReference }).cpuComplete, true); // Saving preserves the curve.
assert.deepEqual(savedReference.totalCostModules, ["Filter"]);
fields.find(field => field.dataset.cpuType === "Reverb").value = "";
saveContext.$("cpu-calibration-save").onclick();
assert.equal(estimate(nodes(["Reverb"]), { calibration: cpuCalibrationFor(model16, "1.5.0", JSON.parse(storage.get("tested-profile"))) }).cpuComplete, false);
const code = source.slice(source.indexOf("function addModule(type, position)"), source.indexOf("function parameterControl("));
assert.ok(code.startsWith("function addModule"));
let draft = { patch: nodes([]) }, failures = [], changes = 0;
const context = { activeDraft: () => draft, moduleSpec: (catalog, type) => catalog.modules.find(s => s.type === type),
  catalog, resourceError: p => estimate(p, { snapshot: { ...tight, psramFree: 300 * KiB, psramLargest: 300 * KiB } }).errors[0] ?? "",
  showError: error => failures.push(error), positions: new Map(), margin: 24, viewport: { scrollLeft: 0, scrollTop: 0 },
  nodeWidth: 132, nodeHeight: () => 52, selectedNode: undefined, changed: () => changes++ };
vm.runInNewContext(code, context);
context.addModule("Reverb", { x: 10, y: 10 });
assert.equal(draft.patch.modules.length, 0); assert.equal(failures.length, 1);
context.addModule("Osc", { x: 10, y: 10 });
assert.equal(draft.patch.modules.length, 1); assert.equal(changes, 1);

// Refresh before upload awaits an already-running poll. A previous device's
// pending response can never overwrite the new device's snapshot.
const refreshCode = source.slice(source.indexOf("function refreshResources()"), source.indexOf("function renderCpuCalibration()"));
const pending = [];
const reader = { port: {}, deviceCatalog: { resourceTelemetry: true }, resourceReading: false,
  resourcePending: undefined, resourceSnapshot: undefined, resourceReadAt: 0, resourceReadError: "",
  validResourceSnapshot, fetchResources: connection => new Promise(resolve => pending.push({ connection, resolve })),
  renderActions() {}, screen: "patches", Date };
vm.runInNewContext(refreshCode, reader);
const firstRead = reader.refreshResources(), concurrentRead = reader.refreshResources();
assert.equal(firstRead, concurrentRead); assert.equal(pending.length, 1);
pending[0].resolve(snapshot); await firstRead;
assert.equal(reader.resourceSnapshot.psramTotal, 8 * MiB);
const stale = reader.refreshResources(); reader.port = {};
const newRead = reader.refreshResources();
pending[2].resolve({ ...snapshot, psramTotal: 16 * MiB }); await newRead;
pending[1].resolve(snapshot); await stale;
assert.equal(reader.resourceSnapshot.psramTotal, 16 * MiB);

// Telemetry polling and user exchanges are serialized, including after ERR.
let stream, inFlight = false, rejectResource = false;
const commands = [], encoder = new TextEncoder(), decoder = new TextDecoder();
const port = {
  readable: new ReadableStream({ start(controller) { stream = controller; } }),
  writable: new WritableStream({ write(bytes) {
    assert.equal(inFlight, false); inFlight = true;
    const command = decoder.decode(bytes).trim(); commands.push(command);
    setTimeout(() => {
      if (command === "RESOURCES" && rejectResource) {
        inFlight = false; stream.enqueue(encoder.encode("ERR storage\n")); return;
      }
      const body = JSON.stringify(command === "RESOURCES" ? snapshot : { version: "1.5.0" });
      inFlight = false; stream.enqueue(encoder.encode(`OK ${encoder.encode(body).length}\n${body}`));
    }, 5);
  } }),
};
const [resources, firmware] = await Promise.all([fetchResources(port), fetchFirmwareInfo(port)]);
assert.ok(validResourceSnapshot(resources)); assert.equal(firmware.version, "1.5.0");
assert.deepEqual(commands, ["RESOURCES", "FWINFO"]);
rejectResource = true;
const results = await Promise.allSettled([fetchResources(port), fetchFirmwareInfo(port)]);
assert.equal(results[0].status, "rejected"); assert.equal(results[1].status, "fulfilled");
assert.equal(port.readable.locked, false); assert.equal(port.writable.locked, false);
console.log("PASS: 8/16 MiB RAM, safety margins, shared/unknown PCM, retained/partial effects, staging/fragmentation, calibrated CPU/voices/unison, add refusal, serialized USB telemetry");
