import { sharedAudioNodes } from "./patch.mjs";

export const KiB = 1024, MiB = 1024 * KiB;
export const ramReserve = 256 * KiB, internalReserve = 48 * KiB;
export const cpuLimit = 85;
export const effectMemory = {
  Delay: { bytes: 352800, largest: 176400 }, Chorus: { bytes: 8192, largest: 8192 },
  Rotary: { bytes: 2048, largest: 2048 }, Reverb: { bytes: 99792, largest: 99792 },
};
const globalControls = new Set(["LFO", "MIDI", "Clock", "Arpeggiator"]);
export const validCpuValue = value => typeof value === "number" && Number.isFinite(value) && value >= 0 && value <= 100;

// User measurements, scoped to the tested hardware and firmware. These are
// average-load observations, not a certified bound for every waveform/modulation.
export function cpuCalibrationFor(model, version, saved = {}) {
  const reference = model === "modulox-brain-s3-32m16m" && ["1.5.0", "1.6.0"].includes(version) ? {
    base: 1.7,
    modules: { Reverb: 36, Filter: 1 },
    // Osc observations include the voice-render path. Interpolate the measured
    // 1/8-note costs rather than multiplying the first-note cost by eight.
    voiceCurves: { Osc: [[1, 8.3], [8, 38.3]] },
    totalCostModules: ["Filter"],
    observations: { notes: [0, 1, 8], dry: [1.7, 10, 40], reverb: [37, 40, 65] },
    referenceFirmware: "1.5.0",
    note: (version === "1.6.0" ? "Extrapolation provisoire des mesures 1.5.0 aux anciens modules de 1.6.0. " : "") + "Référence utilisateur 16 Mio / 1.5.0 : Osc interpolé entre 1 et 8 voix ; Reverb provisionnée à 36 points et Filter à 1 point au total, avant marge. Réglages et pics à confirmer ; les autres modules restent à mesurer.",
  } : {};
  const custom = saved && typeof saved === "object" && !Array.isArray(saved) ? saved : {};
  return { ...reference, ...custom, modules: { ...reference.modules, ...custom.modules },
    // Previously saved scalar measurements were per voice. Do not silently
    // reinterpret them as the new built-in global Filter allowance.
    totalCostModules: custom.totalCostModules ?? reference.totalCostModules?.filter(type => !Object.hasOwn(custom.modules ?? {}, type)) };
}

function curveCost(points, voices) {
  if (!Array.isArray(points) || points.length < 2 || points.some((p, i) =>
    !Array.isArray(p) || p.length !== 2 || !Number.isFinite(p[0]) || !validCpuValue(p[1]) ||
    (i && p[0] <= points[i - 1][0]))) return null;
  if (voices < points[0][0] || voices > points.at(-1)[0]) return null;
  for (let i = 1; i < points.length; ++i) {
    const [a, b] = points[i - 1], [c, d] = points[i];
    if (voices <= c) return b + (d - b) * (voices - a) / (c - a);
  }
  return null;
}

export function validResourceSnapshot(value) {
  const bytes = ["psramTotal", "psramFree", "psramLargest", "internalFree", "internalLargest"];
  return value?.version === 1 && bytes.every(key => Number.isSafeInteger(value[key]) && value[key] >= 0) &&
    value.psramFree <= value.psramTotal && value.psramLargest <= value.psramFree &&
    value.internalLargest <= value.internalFree && typeof value.cpuValid === "boolean" &&
    [value.cpuAverage, value.cpuPeak].every(v => Number.isFinite(v) && v >= 0 && v <= 1000) &&
    Number.isSafeInteger(value.overBudgetBlocks) && value.overBudgetBlocks >= 0 &&
    Object.entries(value.allocatedEffects ?? {}).every(([type, bytes]) => effectMemory[type] &&
      Number.isSafeInteger(bytes) && bytes >= 0 && bytes <= effectMemory[type].bytes);
}

// New sample buffers coexist with the previous patch during preparation.
// Free heap already accounts for that patch, preview/recording and retained effects.
export function estimateResources(patch, catalog, { totalBytes = 8 * MiB, snapshot,
  samples = new Map(), calibration = {} } = {}) {
  const measured = validResourceSnapshot(snapshot);
  const specs = new Map(catalog.modules.map(spec => [spec.type, spec]));
  const shared = sharedAudioNodes(patch, catalog);
  const voices = patch.settings?.["Play mode"] === 1 ? 1 : Math.max(1, Math.min(8, patch.settings?.Voices ?? 1));
  const sampleIds = new Set(), unknownSamples = new Set(), unknownCpu = new Set();
  let extra = 0, margin = 0, largest = 0, sampleBytes = 0, cpu = validCpuValue(calibration.base) ? calibration.base : 0;
  let cpuComplete = validCpuValue(calibration.base);
  const rows = patch.modules.map(node => {
    let bytes = 0, allocation = 0, sizeKnown = true;
    const effect = effectMemory[node.type];
    if (effect) {
      const retained = measured ? Math.min(effect.bytes, Math.max(0, snapshot.allocatedEffects?.[node.type] ?? 0)) : 0;
      bytes = effect.bytes - retained;
      allocation = Math.min(bytes, effect.largest);
    }
    if (["Sampler", "Granulizer"].includes(node.type)) {
      const id = node.sample ?? node.id;
      const sample = samples.get(node.sample);
      const size = sample?.pcm?.byteLength ?? sample?.bytes;
      sizeKnown = Number.isSafeInteger(size) && size > 0;
      if (!sizeKnown) unknownSamples.add(id);
      // An empty Sampler has no PCM and cannot be uploaded as a valid patch.
      // A chosen sample with missing metadata reserves the maximum, never zero.
      const estimate = !node.sample ? 0 : sizeKnown ? size : catalog.maxSampleBytes ?? MiB;
      if (!sampleIds.has(id)) { sampleIds.add(id); bytes += estimate; sampleBytes += estimate; allocation = estimate; }
    }
    // Module state and oscillator cycles are preallocated in internal SRAM.
    // Budget 1 KiB per instance plus 10% on new dynamic buffers as safety margin.
    const safety = KiB + Math.ceil(bytes * 0.1);
    extra += bytes; margin += safety; largest = Math.max(largest, allocation ? allocation + Math.ceil(allocation * 0.1) : 0);
    const cost = calibration.modules?.[node.type];
    const copies = node.type === "Osc" ? node.parameters?.Unison ?? 1 : 1;
    const count = specs.get(node.type)?.sharedEffect || shared.has(node.id) || globalControls.has(node.type) ||
      calibration.totalCostModules?.includes(node.type) ? 1 : voices;
    const curve = !Object.hasOwn(calibration.modules ?? {}, node.type) ? curveCost(calibration.voiceCurves?.[node.type], count) : null;
    const voiceCost = validCpuValue(cost) ? cost * count : curve;
    const moduleCpu = voiceCost === null ? null : voiceCost * (copies > 1 ? copies + 1 : 1) * 1.1;
    if (moduleCpu !== null) cpu += moduleCpu;
    else { unknownCpu.add(node.type); cpuComplete = false; }
    return { id: node.id, type: node.type, bytes, margin: safety, sizeKnown,
      cpu: moduleCpu };
  });
  const total = measured ? snapshot.psramTotal : totalBytes;
  // Offline/old firmware: nominal estimate only, with 1 MiB for existing usage.
  const free = measured ? snapshot.psramFree : Math.max(0, total - MiB);
  const needed = extra + margin + ramReserve;
  const remaining = free - needed;
  const errors = [];
  if (remaining < 0) errors.push("RAM insuffisante : le chargement dépasse le budget avec marge.");
  if (measured && largest > snapshot.psramLargest) errors.push("RAM fragmentée : aucun bloc libre assez grand pour ce sample ou effet.");
  // Do not forbid a small addition merely because SRAM is low; new DSP state is
  // static. SRAM headroom is reported separately, not conflated with PSRAM.
  if (cpu > cpuLimit) errors.push(`CPU estimé ${cpuComplete ? "" : "au moins "}${cpu.toFixed(1)} % : limite de ${cpuLimit} % dépassée.`);
  return { measured, total, free, extra, margin, reserve: ramReserve, needed, remaining,
    used: Math.max(0, total - free), projectedUsed: Math.max(0, total - remaining), largest,
    sampleBytes, unknownSamples: [...unknownSamples], rows, cpu, cpuComplete,
    unknownCpu: [...unknownCpu], errors, blocked: errors.length > 0 };
}
