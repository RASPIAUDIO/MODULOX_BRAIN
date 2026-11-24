import { tr } from "./i18n.mjs";
import { builtinWaves, maxPatchWaves } from "./waves.mjs";
import { maxSampleBytes } from "./samples.mjs";

export const sampleModuleTypes = ["Sampler", "Granulizer"];
export const patchCategories = [
  ["user", "User"], ["bass", "Bass"], ["lead", "Lead"], ["pad", "Pads / Nappes"],
  ["keys", "Keys / Claviers"], ["pluck", "Pluck"], ["arp", "Arp"], ["fx", "FX"], ["drums", "Drums / Percussions"],
];
export const timingModuleTypes = ["Clock", "Arpeggiator", "S&H", "Random", "Gain/Offset", "Slew"];
export const vaModuleTypes = ["Noise", "Sub", "VCA", "Ring Mod", "Wavefolder"];
export const stereoModuleTypes = ["Pan", "Chorus", "Rotary", "Stereo", "Mono", "Stereo Mix"];
export const fm4Algorithms = ["1>2>3>4", "(1+2)>3>4", "(1>2 + 3)>4", "(1 + 2>3)>4",
  "(1>2)+(3>4)", "1>(2+3+4)", "(1>2)+3+4", "1+2+3+4"];
export const fm4Topologies = [ [[0,1,2,4],8], [[0,0,3,4],8], [[0,1,0,6],8], [[0,0,2,5],8],
  [[0,1,0,4],10], [[0,1,1,1],14], [[0,1,0,0],14], [[0,0,0,0],15] ];
export const firstOscillator = patch => patch.modules.find(node => node.type === "Osc");
export const playSettings = ["Play mode", "Glide", "Glide time", "Retrigger"];
export function usesVaExpansion(patch) {
  return patch.modules.some(node => vaModuleTypes.includes(node.type) || node.type === "Osc" &&
    ["Unison", "Detune", "Mix"].some(name => Object.hasOwn(node.parameters ?? {}, name))) ||
    playSettings.some(name => Object.hasOwn(patch.settings ?? {}, name));
}
export const divisions = ["1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars"];

export const offlineCatalog = {
  maxPatches: 64,
  patchCategories: true,
  maxBytes: 4096,
  maxWaves: maxPatchWaves,
  maxSampleBytes,
  midiLearn: true,
  clockRouting: true,
  vaExpansion: true,
  stereoGraph: true,
  fm4: true,
  output: { inputChannels: 2, modulationInputs: ["Pitch", "Level"] },
  settings: { parameters: [
    { name: "Voices", min: 1, max: 8, initial: 1 },
    { name: "Volume", min: 0, max: 100, initial: 77, unit: "%" },
    { name: "MIDI Ch", min: 0, max: 16, initial: 0,
      choices: ["Omni", ...Array.from({ length: 16 }, (_, i) => String(i + 1))] },
    { name: "BPM", min: 20, max: 300, initial: 120 },
    { name: "Clock", min: 0, max: 3, initial: 0, choices: ["Internal", "USB", "DIN", "Sync jack"] },
    { name: "Run", min: 0, max: 1, initial: 1, choices: ["Off", "On"] },
    { name: "Jack PPQN", min: 0, max: 3, initial: 1, choices: ["1", "2", "4", "24"] },
    { name: "Clock loss", min: 0, max: 1, initial: 0, choices: ["Stop", "Hold tempo"] },
    { name: "Play mode", min: 0, max: 1, initial: 0, choices: ["Poly", "Mono"] },
    { name: "Glide", min: 0, max: 2, initial: 0, choices: ["Off", "Always", "Legato"] },
    { name: "Glide time", min: 0, max: 2000, initial: 100, unit: "ms" },
    { name: "Retrigger", min: 0, max: 1, initial: 0, choices: ["Off", "On"] },
  ] },
  modules: [
    { type: "Osc", limit: 3, inputs: 0, modulationInputs: ["Pitch", "Level", "PWM", "FM"],
      midiInputs: ["Pitch", "Velocity", "Trigger"], parameters: [
      { name: "Wave", min: 0, max: builtinWaves.length + maxPatchWaves - 1, initial: 0, choices: builtinWaves },
      { name: "PWM", min: 1, max: 99, initial: 50, unit: "%" },
      { name: "Pitch", min: -24, max: 24, initial: 0, unit: "st", step: 1 },
      { name: "Fine", min: -100, max: 100, initial: 0, unit: "ct" },
      { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Unison", min: 1, max: 4, initial: 1 },
      { name: "Detune", min: 0, max: 100, initial: 15, unit: "ct" },
      { name: "Mix", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "FM", min: 0, max: 100, initial: 0, unit: "%" },
    ] },
    { type: "Filter", limit: 1, inputs: 1, modulationInputs: ["Cutoff", "Reso"], parameters: [
      { name: "Mode", min: 0, max: 3, initial: 0, choices: ["Bypass", "Low-pass", "High-pass", "Band-pass"] },
      { name: "Order", min: 0, max: 1, initial: 0, choices: ["2 poles", "4 poles"] },
      { name: "Cutoff", min: 20, max: 18000, initial: 2000, unit: "Hz" },
      { name: "Reso", min: 0, max: 100, initial: 0, unit: "%" },
    ] },
    { type: "ADSR", limit: 1, inputs: 1, parameters: [
      { name: "Attack", min: 0, max: 2000, initial: 5, unit: "ms" },
      { name: "Decay", min: 0, max: 2000, initial: 120, unit: "ms" },
      { name: "Sustain", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Release", min: 0, max: 4000, initial: 80, unit: "ms" },
    ] },
    { type: "Mix", limit: 1, inputs: 3, parameters: [
      { name: "In 1", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "In 2", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "In 3", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "LFO", limit: 3, inputs: 0, controlOutput: true,
      triggerInputs: ["Reset", "Run"],
      midiInputs: ["Pitch", "Velocity", "Trigger"], parameters: [
      { name: "Wave", min: 0, max: 2, initial: 0, choices: ["Sine", "Square", "Triangle"] },
      { name: "Rate", min: 1, max: 400, initial: 20, unit: "x0.05Hz" },
      { name: "Depth", min: -100, max: 100, initial: 50, unit: "%" },
      { name: "Level", min: -100, max: 100, initial: 0, unit: "%" },
      { name: "Sync", min: 0, max: 1, initial: 0, choices: ["Free", "Sync"] },
      { name: "Division", min: 0, max: 9, initial: 4, choices: divisions },
      { name: "Phase", min: 0, max: 359, initial: 0, unit: "deg" },
    ] },
    { type: "Env", limit: 2, inputs: 0, controlOutput: true, midiInputs: ["Gate", "Trigger"], parameters: [
      { name: "Attack", min: 0, max: 2000, initial: 5, unit: "ms" },
      { name: "Decay", min: 0, max: 2000, initial: 120, unit: "ms" },
      { name: "Sustain", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Release", min: 0, max: 4000, initial: 80, unit: "ms" },
      { name: "Depth", min: -100, max: 100, initial: 50, unit: "%" },
    ] },
    { type: "MIDI", limit: 1, inputs: 0, controlOutput: true,
      midiOutputs: ["Pitch", "Velocity", "Gate", "Trigger"], parameters: [] },
    { type: "Disto", limit: 1, inputs: 1, modulationInputs: ["Mode", "Drive", "Mix"], parameters: [
      { name: "Mode", min: 0, max: 1, initial: 0, choices: ["Soft", "Hard"] },
      { name: "Drive", min: 0, max: 100, initial: 25, unit: "%" },
      { name: "Mix", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "Delay", limit: 1, inputs: 1, inputChannels: 2, outputChannels: 2, sharedEffect: true,
      modulationInputs: ["Time", "Feedback", "Mix", "Time R"], parameters: [
      { name: "Time", min: 1, max: 2000, initial: 220, unit: "ms" },
      { name: "Feedback", min: 0, max: 95, initial: 35, unit: "%" },
      { name: "Mix", min: 0, max: 100, initial: 30, unit: "%" },
      { name: "Sync", min: 0, max: 1, initial: 0, choices: ["Free", "Sync"] },
      { name: "Division", min: 0, max: 9, initial: 4, choices: divisions },
      { name: "Mode", min: 0, max: 2, initial: 0, choices: ["Mono", "Stereo", "Ping-pong"] },
      { name: "Time R", min: 1, max: 2000, initial: 330, unit: "ms" },
      { name: "Division R", min: 0, max: 9, initial: 4, choices: divisions },
      { name: "Link", min: 0, max: 1, initial: 1, choices: ["Off", "On"] },
    ] },
    { type: "Sampler", limit: 3, inputs: 0, parameters: [
      { name: "Root", min: 0, max: 127, initial: 60 },
      { name: "Pitch", min: -24, max: 24, initial: 0, unit: "st" },
      { name: "Fine", min: -100, max: 100, initial: 0, unit: "ct" },
      { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Mode", min: 0, max: 3, initial: 0, choices: ["One-shot", "Gate", "Loop", "Drum"] },
      { name: "Start", min: 0, max: 99, initial: 0, unit: "%" },
      { name: "End", min: 1, max: 100, initial: 100, unit: "%" },
      { name: "MIDI Ch", min: 0, max: 16, initial: 0 },
    ] },
    { type: "Granulizer", limit: 1, inputs: 0,
      modulationInputs: ["Start", "End", "Size", "Density", "Speed", "Spray", "Window", "Pitch", "Level"],
      parameters: [
        { name: "Start", min: 0, max: 99, initial: 0, unit: "%" },
        { name: "End", min: 1, max: 100, initial: 100, unit: "%" },
        { name: "Size", min: 5, max: 300, initial: 80, unit: "ms" },
        { name: "Density", min: 1, max: 60, initial: 20, unit: "Hz" },
        { name: "Speed", min: -200, max: 200, initial: 100, unit: "%" },
        { name: "Spray", min: 0, max: 100, initial: 10, unit: "%" },
        { name: "Window", min: 0, max: 2, initial: 0, choices: ["Hann", "Triangle", "Rect"] },
        { name: "Pitch", min: -24, max: 24, initial: 0, unit: "st" },
        { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
      ] },
    { type: "Clock", limit: 2, inputs: 0, controlOutput: true,
      triggerOutputs: ["Trigger", "Reset", "Run"],
      parameters: [{ name: "Division", min: 0, max: 9, initial: 2, choices: divisions }] },
    { type: "Arpeggiator", limit: 1, inputs: 0, controlOutput: true,
      midiOutputs: ["Pitch", "Velocity", "Gate", "Trigger"], triggerInputs: ["Trigger", "Reset", "Run"],
      parameters: [
        { name: "On", min: 0, max: 1, initial: 1, choices: ["Off", "On"] },
        { name: "Mode", min: 0, max: 4, initial: 0, choices: ["Up", "Down", "Up-down", "Played", "Random"] },
        { name: "Division", min: 0, max: 9, initial: 2, choices: divisions },
        { name: "Gate", min: 1, max: 99, initial: 65, unit: "%" },
        { name: "Octaves", min: 1, max: 4, initial: 1 },
      ] },
    { type: "S&H", limit: 2, inputs: 0, controlOutput: true, cvInputs: ["Value"], triggerInputs: ["Trigger", "Reset"],
      parameters: [{ name: "Initial", min: -100, max: 100, initial: 0, unit: "%" }] },
    { type: "Random", limit: 2, inputs: 0, controlOutput: true, triggerInputs: ["Trigger", "Reset"], parameters: [
      { name: "Depth", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Offset", min: -100, max: 100, initial: 0, unit: "%" },
    ] },
    { type: "Gain/Offset", limit: 1, inputs: 0, controlOutput: true, cvInputs: ["Value"], triggerInputs: ["Reset"], parameters: [
      { name: "Gain", min: -200, max: 200, initial: 100, unit: "%" },
      { name: "Offset", min: -100, max: 100, initial: 0, unit: "%" },
    ] },
    { type: "Slew", limit: 1, inputs: 0, controlOutput: true, cvInputs: ["Value"], triggerInputs: ["Reset"], parameters: [
      { name: "Rise", min: 0, max: 5000, initial: 100, unit: "ms" },
      { name: "Fall", min: 0, max: 5000, initial: 100, unit: "ms" },
    ] },
    { type: "Noise", limit: 1, inputs: 0, modulationInputs: ["Level"], midiInputs: ["Velocity"],
      parameters: [{ name: "Level", min: 0, max: 100, initial: 100, unit: "%" }] },
    { type: "Sub", limit: 1, inputs: 0, modulationInputs: ["Pitch", "Level"], midiInputs: ["Pitch", "Velocity", "Trigger"],
      parameters: [
        { name: "Wave", min: 0, max: 2, initial: 1, choices: ["Sine", "Square", "Triangle"] },
        { name: "Octave", min: 0, max: 1, initial: 0, choices: ["-1", "-2"] },
        { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
      ] },
    { type: "VCA", limit: 1, inputs: 1, modulationInputs: ["Gain"], midiInputs: ["Velocity"], parameters: [
      { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "Depth", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "Ring Mod", limit: 1, inputs: 2, modulationInputs: ["Mix"],
      parameters: [{ name: "Mix", min: 0, max: 100, initial: 100, unit: "%" }] },
    { type: "Wavefolder", limit: 1, inputs: 1, modulationInputs: ["Drive", "Symmetry", "Mix"], parameters: [
      { name: "Drive", min: 0, max: 100, initial: 0, unit: "%" },
      { name: "Symmetry", min: -100, max: 100, initial: 0, unit: "%" },
      { name: "Mix", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "Pan", limit: 3, inputs: 1, outputChannels: 2, modulationInputs: ["Pan", "Level"], parameters: [
      { name: "Pan", min: -100, max: 100, initial: 0, unit: "%" },
      { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "Chorus", limit: 1, inputs: 1, inputChannels: 2, outputChannels: 2, sharedEffect: true,
      modulationInputs: ["Rate", "Depth", "Mix"], parameters: [
        { name: "Rate", min: 5, max: 800, initial: 80, unit: "x0.01Hz" },
        { name: "Depth", min: 0, max: 100, initial: 50, unit: "%" },
        { name: "Mix", min: 0, max: 100, initial: 30, unit: "%" },
      ] },
    { type: "Rotary", limit: 1, inputs: 1, inputChannels: 2, outputChannels: 2, sharedEffect: true,
      modulationInputs: ["Speed", "Depth", "Mix"], parameters: [
        { name: "Speed", min: 0, max: 1, initial: 0, choices: ["Slow", "Fast"] },
        { name: "Slow", min: 10, max: 300, initial: 80, unit: "x0.01Hz" },
        { name: "Fast", min: 100, max: 1200, initial: 650, unit: "x0.01Hz" },
        { name: "Transition", min: 50, max: 5000, initial: 1200, unit: "ms" },
        { name: "Depth", min: 0, max: 100, initial: 70, unit: "%" },
        { name: "Mix", min: 0, max: 100, initial: 100, unit: "%" },
      ] },
    { type: "Stereo", limit: 1, inputs: 2, outputChannels: 2, inputLabels: ["L / Mono", "R"], parameters: [] },
    { type: "Mono", limit: 1, inputs: 1, inputChannels: 2, parameters: [] },
    { type: "Stereo Mix", limit: 1, inputs: 3, inputChannels: 2, outputChannels: 2, parameters: [
      { name: "In 1", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "In 2", min: 0, max: 100, initial: 100, unit: "%" },
      { name: "In 3", min: 0, max: 100, initial: 100, unit: "%" },
    ] },
    { type: "FM4", limit: 1, inputs: 0, modulationInputs: ["Pitch", "Depth", "Feedback", "Level"],
      midiInputs: ["Pitch", "Velocity", "Trigger"], parameters: [
        { name: "Algorithm", min: 0, max: 7, initial: 4, choices: fm4Algorithms },
        { name: "Depth", min: 0, max: 100, initial: 30, unit: "%" },
        { name: "Feedback", min: 0, max: 100, initial: 0, unit: "%" },
        { name: "Level", min: 0, max: 100, initial: 100, unit: "%" },
        { name: "Pitch", min: -24, max: 24, initial: 0, unit: "st" },
        { name: "Fine", min: -100, max: 100, initial: 0, unit: "ct" },
        ...Array.from({length:4}, (_, i) => [
          { name: `${i+1} Ratio`, min: 0, max: 16, initial: i===2?2:1,
            choices: ["0.5","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"] },
          { name: `${i+1} Level`, min: 0, max: 100, initial: [65,90,50,90][i], unit: "%" },
          { name: `${i+1} Attack`, min: 0, max: 2000, initial: 5, unit: "ms" },
          { name: `${i+1} Decay`, min: 0, max: 2000, initial: 300, unit: "ms" },
          { name: `${i+1} Sustain`, min: 0, max: 100, initial: i%2?80:60, unit: "%" },
          { name: `${i+1} Release`, min: 0, max: 4000, initial: 180, unit: "ms" },
        ]).flat(),
      ] },
  ],
};

export const outputModule = { id: "out", type: "Sortie" };
const outputSpec = { type: "Sortie", inputs: 1, inputChannels: 2, parameters: [] };
export const moduleSpec = (catalog, type) =>
  type === outputModule.type ? { ...outputSpec, ...catalog.output } :
    catalog.modules.find((module) => module.type === type);
export function nodeSpec(patch, catalog, node) {
  const spec = moduleSpec(catalog, node?.type);
  if (node?.type !== "Osc" || firstOscillator(patch)?.id === node.id) return spec;
  return { ...spec, parameters: spec.parameters.filter(p => p.name !== "FM"),
    modulationInputs: spec.modulationInputs?.filter(p => p !== "FM") };
}
export function usesFm4(patch) {
  return patch.modules.some(n => n.type === "FM4" || n.type === "Osc" && Object.hasOwn(n.parameters??{},"FM")) ||
    patch.modulations?.some(([,to,name]) => name === "FM" && patch.modules.find(n=>n.id===to)?.type === "Osc");
}

const semitonesPerOctave = 12;

export function pitchCatalog(catalog) {
  const osc = moduleSpec(catalog, "Osc");
  const octave = osc?.parameters.find((parameter) => parameter.name === "Octave");
  if (!octave) return catalog;
  const pitch = {
    ...octave, name: "Pitch", min: octave.min * semitonesPerOctave,
    max: octave.max * semitonesPerOctave, initial: octave.initial * semitonesPerOctave,
    unit: "st", step: semitonesPerOctave,
  };
  return { ...catalog, legacyOctave: true, modules: catalog.modules.map((spec) => spec !== osc ? spec :
    { ...spec, parameters: spec.parameters.map((parameter) => parameter === octave ? pitch : parameter) }) };
}

export function patchForDevice(patch, catalog) {
  if (!catalog.legacyOctave) return patch;
  return { ...patch, modules: patch.modules.map((node) => {
    if (node.type !== "Osc" || !Object.hasOwn(node.parameters ?? {}, "Pitch")) return node;
    const parameters = Object.entries(node.parameters).map(([name, value]) =>
      name === "Pitch" ? ["Octave", value / semitonesPerOctave] : [name, value]);
    return { ...node, parameters: Object.fromEntries(parameters) };
  }) };
}

export function normalizePatch(patch) {
  if (![1, 2, 3].includes(patch?.version) || typeof patch.name !== "string" ||
      !Array.isArray(patch.modules) || !Array.isArray(patch.connections)) {
    throw new Error(tr("Format de patch incompatible."));
  }
  const oscillators = patch.modules.filter((module) => module.type === "Osc").map((module) => module.id);
  const mixes = new Set(patch.modules.filter((module) => module.type === "Mix").map((module) => module.id));
  const normalized = {
    ...patch,
    version: 3,
    modules: patch.modules.map((module) => {
      if (module.type !== "Osc" || !Object.hasOwn(module.parameters ?? {}, "Octave") ||
          Object.hasOwn(module.parameters, "Pitch")) return module;
      const { Octave } = module.parameters;
      if (!Number.isInteger(Octave) || Math.abs(Octave) > 2) return module;
      const parameters = Object.entries(module.parameters).map(([name, value]) =>
        name === "Octave" ? ["Pitch", value * semitonesPerOctave] : [name, value]);
      return { ...module, parameters: Object.fromEntries(parameters) };
    }),
    connections: patch.connections.map(([from, to, inlet]) =>
      [from, to, patch.version === 1 ? (mixes.has(to) ? oscillators.indexOf(from) : 0) : inlet]),
  };
  if (patch.version < 3) {
    const used = new Set(normalized.modules.map(n => n.id));
    const insertStereo = (from, to, input) => {
      let id = "stereo-" + (to === "out" ? "out" : to);
      for (let i = 2; used.has(id); ++i) id = "stereo-convert-" + i;
      used.add(id);
      normalized.modules.push({ id, type: "Stereo" });
      const source = normalized.layout?.[from];
      if (source) normalized.layout = { ...normalized.layout, [id]: {x: source.x, y: source.y + 90} };
      return [[from, id, 0], [id, to, input]];
    };
    normalized.connections = normalized.connections.flatMap(([from, to, input]) => {
      const source = normalized.modules.find(n => n.id === from);
      const target = normalized.modules.find(n => n.id === to);
      const output = moduleSpec(offlineCatalog, source?.type)?.outputChannels ?? 1;
      const inputChannels = to === "out" ? 2 : moduleSpec(offlineCatalog, target?.type)?.inputChannels ?? 1;
      return output === 1 && inputChannels === 2 ? insertStereo(from, to, input) : [[from, to, input]];
    });
  }
  return normalized;
}

export function connectionError(patch, catalog, from, to, input) {
  const source = patch.modules.find((module) => module.id === from);
  const target = to === "out" ? outputModule : patch.modules.find((module) => module.id === to);
  if (!source || moduleSpec(catalog, source.type)?.controlOutput || !target || !Number.isInteger(input) || input < 0 ||
      input >= (moduleSpec(catalog, target.type)?.inputs ?? 0)) return tr("Port audio invalide.");
  const outputChannels = moduleSpec(catalog, source.type)?.outputChannels ?? 1;
  const inputChannels = moduleSpec(catalog, target.type)?.inputChannels ?? 1;
  if (outputChannels !== inputChannels) return outputChannels === 1
    ? tr("Ajoute Pan ou Stereo pour passer de mono à stéréo.")
    : tr("Ajoute Mono pour convertir la stéréo en mono.");
  const visited = new Set();
  const reachesSource = (node) => {
    if (node === from) return true;
    if (visited.has(node)) return false;
    visited.add(node);
    return patch.connections.some(([a, b, port]) =>
      a === node && (b !== to || port !== input) && reachesSource(b));
  };
  return reachesSource(to) ? tr("Ce câble créerait une boucle audio.") : "";
}

export function sharedAudioNodes(patch, catalog) {
  const shared = new Set(patch.modules.filter(n => moduleSpec(catalog, n.type)?.sharedEffect).map(n => n.id));
  for (let i = 0; i < patch.modules.length; ++i) {
    let added = false;
    for (const [from, to] of patch.connections) if (shared.has(from) && !shared.has(to)) { shared.add(to); added = true; }
    if (!added) break;
  }
  return shared;
}

function perVoiceSource(patch, id, visited = new Set()) {
  if (visited.has(id)) return false;
  visited.add(id);
  return patch.modules.find(n => n.id === id)?.type === "Env" ||
    (patch.routings ?? []).some(([from, to, input]) => to === id && input === "Value" && perVoiceSource(patch, from, visited));
}

export function modulationError(patch, catalog, from, to, parameter, midiPort) {
  const source = patch.modules.find((module) => module.id === from);
  const target = to === "out" ? outputModule : patch.modules.find((module) => module.id === to);
  const spec = moduleSpec(catalog, source?.type);
  if (parameter === "FM" && target?.type === "Osc" &&
      (target.id !== firstOscillator(patch)?.id || patch.modules.filter(n=>n.type==="Osc").length < 2))
    return tr("FM relie le premier Osc au deuxième : ajoute deux oscillateurs.");
  const shared = sharedAudioNodes(patch, catalog).has(to);
  if (midiPort !== undefined) {
    if (shared) return tr("Les commandes par voix doivent précéder les effets communs.");
    if (!spec?.midiOutputs?.includes(midiPort) || midiPort !== parameter ||
        !moduleSpec(catalog, target?.type)?.midiInputs?.includes(parameter)) {
      return tr("Relie une sortie MIDI à l’entrée de même nom.");
    }
    return "";
  }
  if (!spec?.controlOutput || spec.midiOutputs || spec.triggerOutputs?.length ||
      !moduleSpec(catalog, target?.type)?.modulationInputs?.includes(parameter)) {
    return tr("Relie le LFO ou l’enveloppe à une entrée de modulation bleue.");
  }
  if ((target.type === "Disto" || shared) && perVoiceSource(patch, from)) {
    return tr("Cette entrée d’effet accepte une modulation globale.");
  }
  return "";
}

export function routingError(patch, catalog, from, to, input, output) {
  const source = patch.modules.find((node) => node.id === from);
  const target = patch.modules.find((node) => node.id === to);
  const spec = moduleSpec(catalog, source?.type);
  const destination = moduleSpec(catalog, target?.type);
  if (!catalog.clockRouting || !spec || !destination || from === to) return tr("Routage de contrôle invalide.");
  if (input === "Value") {
    if (output !== "CV" || !destination.cvInputs?.includes(input) || !spec.controlOutput ||
        spec.midiOutputs || spec.triggerOutputs?.length) return tr("Relie une valeur CV à cette entrée.");
    const seen = new Set();
    function dependsOn(id) {
      if (id === to) return true;
      if (seen.has(id)) return false;
      seen.add(id);
      return (patch.routings ?? []).some(([a, b, port]) => b === id && port === "Value" && dependsOn(a));
    }
    if (dependsOn(from)) return tr("Ce câble créerait une boucle de modulation.");
  } else if (!destination.triggerInputs?.includes(input) ||
      !(spec.triggerOutputs?.includes(output) || (spec.midiOutputs?.includes(output) && output === "Trigger")) ||
      (input === "Run" ? output !== "Run" : output === "Run")) {
    return tr("Relie une impulsion à Trigger ou Reset, et Run à Run.");
  }
  return "";
}

export function validatePatch(patch, catalog, { allowIncomplete = false } = {}) {
  if (patch.category !== undefined && !patchCategories.some(([id]) => id === patch.category)) {
    return tr("Catégorie de patch invalide.");
  }
  if (patch.version !== 3) return tr("Ouvre ce patch pour adapter son graphe à la stéréo.");
  const name = patch.name.trim();
  if (!name || new TextEncoder().encode(name).length > 31 || /[\x00-\x1f]/.test(name)) {
    return tr("Le nom doit faire entre 1 et 31 octets UTF-8.");
  }
  const waves = patch.waves ?? [];
  if (!Array.isArray(waves) || waves.some((id) => typeof id !== "string" || !/^[a-f0-9]{14}$/.test(id)) ||
      new Set(waves).size !== waves.length || waves.length > (catalog.maxWaves ?? 0)) {
    return tr("La banque de formes d’onde est incompatible avec ce firmware.");
  }
  for (const [name, value] of Object.entries(patch.settings ?? {})) {
    const parameter = catalog.settings?.parameters.find((item) => item.name === name);
    if (!parameter || !Number.isInteger(value) || value < parameter.min || value > parameter.max) {
      return tr("Réglage incompatible avec le firmware · {0}.", name);
    }
  }
  const nodes = new Map();
  const counts = new Map();
  for (const node of patch.modules) {
    const spec = moduleSpec(catalog, node.type);
    if (sampleModuleTypes.includes(node.type) && (!catalog.maxSampleBytes ||
        !(allowIncomplete && !node.sample) && !/^[a-f0-9]{14}$/.test(node.sample ?? ""))) {
      return tr("Choisis un sample pour {0} sur un firmware compatible.", node.id);
    }
    if (!spec?.limit || !/^[\w-]{1,31}$/.test(node.id) || node.id === "out" || nodes.has(node.id)) {
      return tr("Identifiant ou type de module invalide.");
    }
    nodes.set(node.id, node);
    counts.set(node.type, (counts.get(node.type) ?? 0) + 1);
    if (counts.get(node.type) > spec.limit) return tr("Trop de modules {0}.", node.type);
    for (const [name, value] of Object.entries(node.parameters ?? {})) {
      const parameter = spec.parameters.find((item) => item.name === name);
      if (!parameter || !Number.isInteger(value) || value < parameter.min || value > parameter.max) {
        return tr("Valeur invalide pour {0} · {1}.", node.id, name);
      }
      if (node.type === "Osc" && name === "FM" && node.id !== firstOscillator(patch)?.id)
        return tr("FM se règle sur le premier Osc uniquement.");
      if (catalog.legacyOctave && node.type === "Osc" && name === "Pitch" &&
          value % semitonesPerOctave !== 0) {
        return tr("Cette version du firmware accepte uniquement les multiples de 12 demi-tons pour Pitch.");
      }
      if (node.type === "Osc" && name === "Wave" && value >= builtinWaves.length + waves.length) {
        return tr("L’oscillateur utilise une forme d’onde absente du patch.");
      }
    }
  }
  const incoming = new Map();
  const oscillators = patch.modules.filter(n=>n.type==="Osc");
  if ((oscillators[0]?.parameters?.FM??0) > 0 && oscillators.length < 2) return tr("Ajoute le deuxième Osc pour activer FM.");
  const learned = new Set();
  for (const [id, name, cc] of patch.midiLearn ?? []) {
    const spec = id === "$settings" ? catalog.settings : moduleSpec(catalog, nodes.get(id)?.type);
    const key = id + "/" + name;
    if (!catalog.midiLearn || learned.has(key) || !Number.isInteger(cc) || cc < 0 || cc >= 120 ||
        !spec?.parameters.some((parameter) => parameter.name === name)) {
      return tr("Affectation MIDI Learn incompatible.");
    }
    learned.add(key);
  }
  for (const [from, to, input] of patch.connections) {
    const error = connectionError(patch, catalog, from, to, input);
    if (error) return error;
    const ports = incoming.get(to) ?? new Map();
    if (ports.has(input)) return tr("Une entrée ne peut recevoir qu’un câble.");
    ports.set(input, from);
    incoming.set(to, ports);
  }
  if (!allowIncomplete && !incoming.has("out")) return tr("Relie un module à la Sortie.");
  const used = new Set();
  function visit(id) {
    for (const from of incoming.get(id)?.values() ?? []) {
      if (!used.has(from)) {
        used.add(from);
        visit(from);
      }
    }
  }
  visit("out");
  if (oscillators.length > 1 && used.has(oscillators[0].id)) used.add(oscillators[1].id);
  const modulationSources = new Set();
  const modulationTargets = new Set();
  for (const [from, to, parameter, midiPort] of patch.modulations ?? []) {
    const error = modulationError(patch, catalog, from, to, parameter, midiPort);
    if (error) return error;
    const destination = to + "/" + (midiPort === undefined ? parameter : "MIDI/" +
      (["Gate", "Trigger"].includes(parameter) && nodes.get(to)?.type === "Env" ? "Gate" : parameter));
    if (modulationTargets.has(destination)) return tr("Une entrée de modulation accepte un seul câble.");
    modulationTargets.add(destination);
    modulationSources.add(from);
  }
  const routingTargets = new Set();
  for (const edge of patch.routings ?? []) {
    if (!Array.isArray(edge) || edge.length !== 4) return tr("Routage invalide.");
    const [from, to, input, output] = edge;
    const error = routingError(patch, catalog, from, to, input, output);
    if (error) return error;
    const key = to + "/" + input;
    if (routingTargets.has(key)) return tr("Une entrée de contrôle accepte un seul câble.");
    routingTargets.add(key);
    modulationSources.add(from);
    modulationSources.add(to);
  }
  const shared = sharedAudioNodes(patch, catalog);
  for (const node of patch.modules) if (shared.has(node.id) && node.type === "ADSR")
    return tr("Place l’ADSR avant les effets communs.");
  for (const [from, to] of patch.modulations ?? []) {
    if ((nodes.get(to)?.type === "Disto" || shared.has(to)) && perVoiceSource(patch, from)) {
      return tr("Les effets communs demandent une modulation globale.");
    }
  }
  for (const node of nodes.values()) {
    if (moduleSpec(catalog, node.type).controlOutput) {
      if (!allowIncomplete && !modulationSources.has(node.id)) return tr("Relie la sortie de {0}.", node.id);
      continue;
    }
    if (!allowIncomplete && !used.has(node.id)) return tr("{0} n’est pas relié à la Sortie.", node.id);
    if (!allowIncomplete && moduleSpec(catalog, node.type).inputs && !incoming.has(node.id)) {
      return tr("Branche une entrée de {0}.", node.id);
    }
    if (!allowIncomplete && node.type === tr("Ring Mod") && incoming.get(node.id)?.size !== 2) return tr("Branche les deux entrées audio du Ring Mod.");
  }
  return allowIncomplete || ["Osc", "Noise", "Sub", "FM4", ...sampleModuleTypes].some((type) => counts.get(type)) ? "" : tr("Ajoute une source audio.");
}
