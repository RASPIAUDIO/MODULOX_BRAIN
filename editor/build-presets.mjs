// Rebuild the authored bank without replacing the validated first preset.
import { readFile, writeFile, mkdir, readdir } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { readWave, waveId } from "./dist/waves.mjs";
import { validatePatch, offlineCatalog } from "./dist/patch.mjs";

const root = new URL("./dist/presets/", import.meta.url);
const assets = new URL("assets/", root);
await mkdir(assets, { recursive: true });
const manifest = { waves: [], samples: [] };
async function wave(folder, file, name) {
  const source = `AKWF/${folder}/${file}.wav`;
  const bytes = await readFile(new URL(source, import.meta.url));
  const decoded = await readWave({ name: file + ".wav", arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) });
  await writeFile(new URL(decoded.id + ".pcm", assets), decoded.pcm);
  manifest.waves.push({ id: decoded.id, name, source });
  return decoded.id;
}
const saw = await wave("AKWF_bw_saw", "AKWF_saw_0001", "AKWF Saw 1");
const strings = await wave("AKWF_stringbox", "AKWF_cheeze_0001", "AKWF Stringbox Cheeze 1");
const vocal = await wave("AKWF_hvoice", "AKWF_hvoice_0001", "AKWF Hvoice 1");
const digital = await wave("AKWF_granular", "AKWF_granular_0001", "AKWF Granular 1");

let seed = 0x197a11;
const random = () => { seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5; return (seed >>> 0) / 2147483648 - 1; };
async function texture(name, kind) {
  const count = 44100 * 3;
  const floats = new Float64Array(count);
  let peak = 0, noise = 0;
  for (let i = 0; i < count; i++) {
    const t = i / 44100, phase = 2 * Math.PI * 261.625565 * t;
    noise += (random() - noise) * 0.13;
    let value;
    if (kind === "choir") value = (Math.sin(phase) + 0.48 * Math.sin(phase * 2 + 0.2 * Math.sin(t * 2)) +
      0.32 * Math.sin(phase * 3) + 0.17 * Math.sin(phase * 5)) * (0.7 + 0.16 * Math.sin(t * 3.1)) + noise * 0.06;
    else if (kind === "metal") value = (Math.sin(phase) + 0.3 * Math.sin(phase * 2.76) +
      0.2 * Math.sin(phase * 4.13) + 0.1 * Math.sin(phase * 6.71)) * (0.65 + 0.25 * Math.sin(t * 1.8)) + noise * 0.08;
    else value = (0.25 * Math.sin(phase) + 0.35 * Math.sin(phase * 1.501) + noise * 0.5) *
      Math.pow(0.5 + 0.5 * Math.sin(t * 27), 8) * (0.7 + 0.3 * Math.sin(t * 1.3));
    const fade = Math.min(1, i / 2205, (count - 1 - i) / 2205);
    floats[i] = value * fade;
    peak = Math.max(peak, Math.abs(floats[i]));
  }
  const pcm = new Uint8Array(count * 2), view = new DataView(pcm.buffer);
  for (let i = 0; i < count; i++) view.setInt16(i * 2, Math.round(floats[i] / peak * 24575), true);
  const id = await waveId(pcm);
  await writeFile(new URL(id + ".pcm", assets), pcm);
  manifest.samples.push({ id, name, bytes: pcm.length, sampleRate: 44100, root: 60, source: "Synthese originale deterministe, build-presets.mjs" });
  return id;
}
const choirSample = await texture("Choir C4 Texture", "choir");
const metalSample = await texture("Bowed Metal C4", "metal");
const dustSample = await texture("Pulsed Dust C4", "dust");
await writeFile(new URL("index.json", assets), JSON.stringify(manifest, null, 2) + "\n");
await writeFile(new URL("AKWF-CREDITS.txt", assets), await readFile(new URL("AKWF/AKWF_bw_saw/AdventureKidWaveforms.txt", import.meta.url)));

const bank = [];
const adsr = (Attack = 4, Decay = 180, Sustain = 80, Release = 100) => ({ Attack, Decay, Sustain, Release });
const osc = (wave = 1, Pitch = 0, Fine = 0, Level = 75, extra = {}) => ({ wave, Pitch, Fine, Level, ...extra });
class Patch {
  constructor(name, category, description, voices = 4) {
    this.patch = { version: 3, name, category: { Basses: "bass", Leads: "lead", Nappes: "pad", Claviers: "keys", FX: "fx", Percussions: "drums" }[category], settings: { Voices: voices, Volume: 85, "MIDI Ch": 0, BPM: 120 }, modules: [], connections: [] };
    this.category = category; this.description = description; this.ids = new Map(); this.row = 0;
  }
  node(type, parameters = {}, extra = {}) {
    const count = (this.ids.get(type) ?? 0) + 1; this.ids.set(type, count);
    const id = type.toLowerCase().replace(/[^a-z0-9]/g, "") + count;
    this.patch.modules.push({ id, type, ...(Object.keys(parameters).length ? { parameters } : {}), ...extra });
    return id;
  }
  connect(from, to, input = 0) { this.patch.connections.push([from, to, input]); return to; }
  mod(from, to, name) { (this.patch.modulations ??= []).push([from, to, name]); }
  route(from, to, input, output) { (this.patch.routings ??= []).push([from, to, input, output]); }
  oscillator(config) {
    const { wave, ...parameters } = config;
    let Wave = wave;
    if (typeof wave === "string") {
      const waves = this.patch.waves ??= [];
      if (!waves.includes(wave)) waves.push(wave);
      Wave = 3 + waves.indexOf(wave);
    }
    return this.node("Osc", { Wave, PWM: 50, Unison: 1, ...parameters });
  }
  lfo(target, parameter, Rate, Depth, extra = {}) {
    const id = this.node("LFO", { Wave: 0, Rate, Depth, Level: 0, ...extra });
    this.mod(id, target, parameter); return id;
  }
  env(target, parameter, depth, shape) {
    const id = this.node("Env", { ...shape, Depth: depth }); this.mod(id, target, parameter); return id;
  }
  effect(tail, type, parameters) { return this.connect(tail, this.node(type, parameters)); }
  finish(tail, { chorus, delay, rotary, pan = 0 } = {}) {
    tail = this.effect(tail, "Pan", { Pan: pan, Level: 85 });
    if (chorus) tail = this.effect(tail, "Chorus", { Rate: 65, Depth: 35, Mix: 20, ...chorus });
    if (rotary) tail = this.effect(tail, "Rotary", { Speed: 0, Slow: 70, Fast: 620, Transition: 900, Depth: 60, Mix: 65, ...rotary });
    if (delay) tail = this.effect(tail, "Delay", { Time: 300, "Time R": 450, Feedback: 25, Mix: 18, Mode: 2, Link: 0, ...delay });
    this.connect(tail, "out");
    // Arrange audio sources across a row, then route downwards. CV sources
    // sit to the right; every patch remains movable in the taller workspace.
    const layout = {}; const depths = new Map();
    const controls = this.patch.modules.filter(n => offlineCatalog.modules.find(s => s.type === n.type)?.controlOutput);
    const audio = this.patch.modules.filter(n => !controls.includes(n));
    const pending = [...audio, { id: "out", type: "Sortie" }];
    while (pending.length) {
      const ready = pending.filter(n => this.patch.connections.filter(e => e[1] === n.id).every(e => depths.has(e[0])));
      if (!ready.length) throw new Error("Audio cycle");
      for (const n of ready) {
        const depth = Math.max(-1, ...this.patch.connections.filter(e => e[1] === n.id).map(e => depths.get(e[0]))) + 1;
        const peers = Object.keys(layout).filter(id => depths.get(id) === depth).length;
        depths.set(n.id, depth); layout[n.id] = { x: 24 + peers * 180, y: 24 + depth * 140 };
        pending.splice(pending.indexOf(n), 1);
      }
    }
    controls.forEach((n, i) => { layout[n.id] = { x: 620 + (i % 2) * 180, y: 24 + Math.floor(i / 2) * 160 }; });
    this.patch.layout = layout;
    const error = validatePatch(this.patch, offlineCatalog);
    if (error) throw new Error(this.patch.name + ": " + error);
    const size = Buffer.byteLength(JSON.stringify(this.patch));
    if (size > 4096) throw new Error(this.patch.name + ": " + size + " bytes");
    bank.push(this); return this;
  }
}

function va(name, category, description, c = {}) {
  const p = new Patch(name, category, description, c.voices ?? (category === "Basses" ? 1 : 3));
  if (category === "Basses" || c.mono) Object.assign(p.patch.settings, { "Play mode": 1, Glide: 2, "Glide time": c.glide ?? 65, Retrigger: c.retrigger ?? 0 });
  const oscs = (c.oscs ?? [osc(saw, 0, -5), osc(saw, 0, 5), osc(2, -12, 0, 80)]).map(o => p.oscillator(o));
  const mix = p.node("Mix", { "In 1": 100, "In 2": 100, "In 3": 100 });
  oscs.forEach((o, i) => p.connect(o, mix, i));
  let tail = mix;
  if (c.folder) tail = p.effect(tail, "Wavefolder", c.folder);
  const filter = p.node("Filter", { Mode: c.mode ?? 1, Order: c.order ?? 1, Cutoff: c.cutoff ?? 1200, Reso: c.reso ?? 16 });
  tail = p.connect(tail, filter);
  if (c.env !== false) p.env(filter, "Cutoff", c.depth ?? 45, c.env ?? adsr(3, 260, 12, 140));
  if (c.wobble) p.lfo(filter, "Cutoff", c.wobble.rate ?? 32, c.wobble.depth ?? 65, { Sync: 1, Division: c.wobble.division ?? 4, Wave: c.wobble.wave ?? 0 });
  if (c.pwm) p.lfo(oscs[0], "PWM", c.pwm.rate ?? 5, c.pwm.depth ?? 25);
  if (c.vibrato) { const lfo = p.lfo(oscs[0], "Pitch", 108, c.vibrato); oscs.slice(1).forEach(o => p.mod(lfo, o, "Pitch")); }
  tail = p.effect(tail, "ADSR", c.amp ?? adsr());
  if (c.disto) tail = p.effect(tail, "Disto", { Mode: 0, Drive: 30, Mix: 30, ...c.disto });
  p.finish(tail, c); return p;
}

// 02–09: VA basses, three actual oscillators in every patch.
va("VA Analo Bass", "Basses", "Basse ronde : deux saws, triangle grave, enveloppe courte.", { cutoff: 900, depth: 40, amp: adsr(4, 180, 70, 110), disto: { Drive: 18, Mix: 22 } });
va("VA Wobble Bass", "Basses", "Balayage de filtre synchronise a la croche, distorsion douce.", { env: false, cutoff: 1600, reso: 28, wobble: { division: 4, depth: 72 }, disto: { Drive: 42, Mix: 55 }, amp: adsr(5, 90, 100, 100) });
va("VA Acid Pulse", "Basses", "Carres et sinus grave, resonance et morsure percussive.", { oscs: [osc(1, 0, 0, 85, { PWM: 35 }), osc(1, 0, 4, 55, { PWM: 62 }), osc(0, -12, 0, 80)], cutoff: 1100, reso: 62, depth: 65, env: adsr(0, 190, 0, 90), retrigger: 1, disto: { Drive: 35, Mix: 42 } });
va("VA Reese Motion", "Basses", "Saws desaccordees en unisson, mouvement lent et chorus leger.", { oscs: [osc(saw, 0, -12, 72, { Unison: 2, Detune: 10, Mix: 55 }), osc(saw, 0, 12, 72), osc(0, -12, 0, 90)], env: false, cutoff: 1900, wobble: { rate: 4, depth: 14, division: 9 }, chorus: { Rate: 24, Depth: 18, Mix: 12 }, disto: { Drive: 26, Mix: 35 } });
va("VA Rubber Bass", "Basses", "Pulse et triangles : filtre elastique, attaque courte et glide appuye.", { oscs: [osc(1, 0, -2, 78, { PWM: 24 }), osc(2, 0, 3, 75), osc(0, -12, 0, 90)], cutoff: 1000, reso: 38, depth: 68, env: adsr(8, 330, 5, 100), glide: 120, retrigger: 1 });
va("VA Dub Sub", "Basses", "Sinus grave et triangles, filtre ferme, echo discret.", { oscs: [osc(0, -12, 0, 100), osc(2, 0, 0, 45), osc(2, -12, -2, 55)], cutoff: 550, depth: 20, amp: adsr(8, 300, 65, 220), delay: { Time: 375, "Time R": 375, Feedback: 24, Mix: 10, Mode: 0 } });
va("VA Hard Bite", "Basses", "Saws et pulse, wavefolder modere puis saturation franche.", { oscs: [osc(saw, 0, -3, 70), osc(1, 0, 3, 65, { PWM: 42 }), osc(2, -12, 0, 80)], folder: { Drive: 12, Symmetry: 0, Mix: 24 }, cutoff: 1800, depth: 50, disto: { Mode: 1, Drive: 30, Mix: 40 } });
va("VA Pulse Driver", "Basses", "Basse pulse avec PWM lente, tenue stable et attaque de filtre.", { oscs: [osc(1, 0, -4, 80, { PWM: 40 }), osc(1, 0, 4, 70, { PWM: 58 }), osc(0, -12, 0, 90)], pwm: { rate: 8, depth: 22 }, cutoff: 1300, depth: 38, disto: { Drive: 22, Mix: 30 } });

// 10–17: VA leads, likewise three oscillators, with tailored stereo effects.
va("VA Velvet Solo", "Leads", "Lead mono doux, vibrato discret, glide legato et echo stereo.", { mono: true, cutoff: 2400, depth: 18, oscs: [osc(2, 0, -4, 85), osc(saw, 0, 4, 50), osc(0, 0, 0, 65)], vibrato: 2, amp: adsr(16, 260, 80, 210), delay: { Mix: 20 } });
va("VA Wide Saw", "Leads", "Trois saws, unisson dose, chorus et ping-pong.", { oscs: [osc(saw, 0, -7, 70, { Unison: 3, Detune: 18, Mix: 65 }), osc(saw, 0, 7, 65), osc(saw, 12, 0, 40)], cutoff: 5100, depth: 22, chorus: { Mix: 22 }, delay: { Mix: 22, Feedback: 30 }, amp: adsr(12, 300, 80, 260) });
va("VA PWM Runner", "Leads", "Lead pulse articule, PWM lente et delay synchronise.", { oscs: [osc(1, 0, -3, 80, { PWM: 32 }), osc(1, 0, 3, 65, { PWM: 62 }), osc(2, 12, 0, 38)], cutoff: 3800, depth: 34, pwm: { rate: 11, depth: 30 }, amp: adsr(3, 180, 55, 140), delay: { Sync: 1, Division: 3, "Division R": 4, Mix: 22 } });
va("VA Acid Scream", "Leads", "Lead mono resonant, distorsion et echo court.", { mono: true, oscs: [osc(saw, 0, 0, 80), osc(1, 0, 6, 60, { PWM: 30 }), osc(2, 12, 0, 45)], cutoff: 2400, reso: 58, depth: 52, env: adsr(0, 260, 18, 140), retrigger: 1, disto: { Drive: 42, Mix: 50 }, delay: { Time: 190, "Time R": 285, Mix: 16 } });
va("VA Octave Stack", "Leads", "Saws sur trois octaves pour un solo present et large.", { oscs: [osc(saw, 0, -3, 85), osc(saw, 12, 3, 55), osc(1, -12, 0, 45)], cutoff: 4600, depth: 25, chorus: { Depth: 22, Mix: 15 }, delay: { Mix: 20 } });
va("VA Glass Glide", "Leads", "Sinus et triangles, attaque douce et long glide de 180 ms.", { mono: true, glide: 180, oscs: [osc(0, 0, 0, 90), osc(2, 12, 3, 45), osc(2, 0, -3, 65)], cutoff: 3200, depth: 10, amp: adsr(24, 400, 75, 340), chorus: { Rate: 30, Depth: 25, Mix: 20 }, delay: { Mix: 24, Feedback: 32 } });
va("VA Retro Brass", "Leads", "Attaque de cuivre, saws legerement detunees et chorus vintage.", { oscs: [osc(saw, 0, -5, 80), osc(saw, 0, 5, 75), osc(1, 0, 0, 40)], cutoff: 1800, depth: 48, env: adsr(75, 400, 45, 220), amp: adsr(35, 300, 85, 200), chorus: { Rate: 90, Depth: 28, Mix: 23 }, voices: 4 });
va("VA Fifth Flight", "Leads", "Lead a la quinte avec pulse et saw, ping-pong ample.", { oscs: [osc(saw, 0, -4, 80), osc(1, 7, 3, 48, { PWM: 45 }), osc(2, 12, 0, 42)], cutoff: 4200, depth: 30, delay: { Time: 375, "Time R": 500, Mix: 24, Feedback: 32 }, amp: adsr(8, 220, 70, 240) });

// Pads: VA, wavetable, FM and supplied granular textures.
va("PAD Analog Dawn", "Nappes", "Trois saws, ouverture lente, unisson et chorus ample.", { voices: 4, oscs: [osc(saw, 0, -8, 65, { Unison: 2, Detune: 15, Mix: 60 }), osc(saw, 0, 8, 65), osc(2, -12, 0, 55)], cutoff: 1100, depth: 42, env: adsr(1500, 1500, 75, 2300), amp: adsr(1700, 1500, 85, 2700), chorus: { Rate: 24, Depth: 60, Mix: 35 }, delay: { Mix: 15, Feedback: 32 } });

function fmParameters(c = {}) {
  const parameters = { Algorithm: c.algorithm ?? 4, Depth: c.depth ?? 16, Feedback: c.feedback ?? 0, Level: c.level ?? 75, Pitch: c.pitch ?? 0 };
  for (let i = 0; i < 4; i++) {
    const op = c.ops?.[i] ?? {};
    const shape = op.shape ?? (i % 2 === 0 ? adsr(1, 450, 0, 150) : adsr(3, 900, 15, 400));
    parameters[`${i + 1} Ratio`] = op.ratio ?? [1, 1, 3, 1][i];
    parameters[`${i + 1} Level`] = op.level ?? [55, 90, 40, 90][i];
    for (const [name, value] of Object.entries(shape)) parameters[`${i + 1} ${name}`] = value;
  }
  return parameters;
}
function fm(name, category, description, c = {}) {
  const p = new Patch(name, category, description, c.voices ?? 4);
  const source = p.node("FM4", fmParameters(c));
  let tail = source;
  if (c.filter) tail = p.effect(tail, "Filter", { Mode: 1, Order: 0, Cutoff: 5000, Reso: 5, ...c.filter });
  if (c.disto) tail = p.effect(tail, "Disto", { Mode: 0, Drive: 16, Mix: 20, ...c.disto });
  if (c.lfo) p.lfo(source, c.lfo.target ?? "Depth", c.lfo.rate ?? 5, c.lfo.depth ?? 5);
  if (c.pitchEnv) p.env(source, "Pitch", c.pitchEnv.depth, c.pitchEnv.shape);
  p.finish(tail, c); return p;
}
fm("PAD Glass Choir", "Nappes", "FM douce a enveloppes longues, timbre anime et echo stereo.", { depth: 10, ops: [
  { ratio: 2, level: 32, shape: adsr(1600, 1200, 65, 2400) }, { ratio: 1, level: 85, shape: adsr(1300, 1600, 85, 2800) },
  { ratio: 3, level: 20, shape: adsr(2000, 1200, 55, 2600) }, { ratio: 1, level: 85, shape: adsr(1500, 1800, 85, 3000) }], lfo: { rate: 3, depth: 2 }, chorus: { Rate: 20, Depth: 50, Mix: 32 }, delay: { Mix: 21, Feedback: 35 } });
va("PAD Orbit Strings", "Nappes", "Tables AKWF Stringbox avec vibrato lent, filtre doux et chorus.", { voices: 4, oscs: [osc(strings, 0, -6, 65), osc(strings, 0, 6, 65), osc(2, -12, 0, 40)], cutoff: 2400, depth: 20, vibrato: 1, env: adsr(1400, 1800, 70, 2600), amp: adsr(850, 1300, 90, 2500), chorus: { Rate: 35, Depth: 55, Mix: 36 }, delay: { Mix: 15 } });
function grain(name, category, description, sample, c = {}) {
  const p = new Patch(name, category, description, c.voices ?? 2);
  const source = p.node("Granulizer", { Start: 5, End: 95, Size: 160, Density: 24, Speed: 25, Spray: 25, Window: 0, Pitch: 0, Level: 75, ...c.grain }, { sample });
  let tail = source;
  if (c.lfo) p.lfo(source, c.lfo.target, c.lfo.rate, c.lfo.depth);
  if (c.filter) tail = p.effect(tail, "Filter", { Mode: 1, Order: 0, Cutoff: 4500, Reso: 10, ...c.filter });
  tail = p.effect(tail, "ADSR", c.amp ?? adsr(1200, 800, 100, 2000));
  if (c.disto) tail = p.effect(tail, "Disto", { Mode: 0, Drive: 20, Mix: 20, ...c.disto });
  p.finish(tail, c); return p;
}
grain("PAD Grain Aurora", "Nappes", "Texture vocale C4 fournie, grains longs, balayage lent et stereo.", choirSample, { lfo: { target: "Start", rate: 2, depth: 8 }, chorus: { Rate: 18, Depth: 48, Mix: 30 }, delay: { Mix: 25, Feedback: 38 } });
va("PAD Frozen Voices", "Nappes", "Tables vocales AKWF sur trois octaves, filtre lent et echo long.", { voices: 3, oscs: [osc(vocal, 0, -4, 70), osc(vocal, 12, 4, 36), osc(0, -12, 0, 50)], cutoff: 2000, depth: 25, env: adsr(1800, 1600, 80, 3000), amp: adsr(1600, 1600, 90, 3000), chorus: { Rate: 16, Depth: 45, Mix: 33 }, delay: { Time: 650, "Time R": 890, Feedback: 38, Mix: 26 } });
va("PAD Midnight Drone", "Nappes", "Triangles et table granulaire, wavefolder leger et mouvement lent.", { voices: 2, oscs: [osc(2, -12, -4, 70), osc(digital, 0, 4, 45), osc(0, -12, 0, 70)], folder: { Drive: 9, Symmetry: 4, Mix: 30 }, cutoff: 1400, env: false, wobble: { depth: 15, division: 9 }, amp: adsr(2000, 1800, 100, 3500), chorus: { Mix: 28, Rate: 12, Depth: 50 }, delay: { Mix: 18, Feedback: 35 } });
fm("PAD Shimmer Tides", "Nappes", "FM additive aux harmoniques douces, montee lente et ping-pong.", { algorithm: 7, depth: 0, ops: [
  { ratio: 1, level: 85, shape: adsr(1400, 1800, 80, 3000) }, { ratio: 2, level: 40, shape: adsr(1800, 1800, 70, 2800) },
  { ratio: 3, level: 22, shape: adsr(2000, 1800, 50, 3200) }, { ratio: 4, level: 12, shape: adsr(1700, 1800, 45, 3400) }], chorus: { Rate: 20, Depth: 65, Mix: 35 }, delay: { Time: 500, "Time R": 750, Feedback: 40, Mix: 26 } });

// Eight FM keyboards: individual modulator/carrier envelopes shape each attack.
fm("KEY FM Clav Dry", "Claviers", "Clavinet FM sec et court, attaques brillantes et filtre doux.", { depth: 20, feedback: 8, ops: [
  { ratio: 3, level: 62, shape: adsr(0, 100, 0, 60) }, { ratio: 1, level: 90, shape: adsr(1, 350, 0, 70) },
  { ratio: 2, level: 38, shape: adsr(0, 80, 0, 50) }, { ratio: 1, level: 75, shape: adsr(1, 260, 0, 60) }], filter: { Cutoff: 6500 }, disto: { Drive: 12, Mix: 18 } });
fm("KEY FM Clav Funk", "Claviers", "Clavinet FM claquant avec saturation legere, rotary rapide et slap.", { depth: 24, feedback: 10, ops: [
  { ratio: 4, level: 60, shape: adsr(0, 120, 0, 70) }, { ratio: 1, level: 90, shape: adsr(1, 420, 0, 90) },
  { ratio: 2, level: 40, shape: adsr(0, 90, 0, 60) }, { ratio: 1, level: 80, shape: adsr(1, 310, 0, 75) }], disto: { Drive: 20, Mix: 25 }, rotary: { Speed: 1, Depth: 40, Mix: 45 }, delay: { Time: 90, "Time R": 125, Feedback: 8, Mix: 10 } });
fm("KEY EP Velvet", "Claviers", "Piano electrique doux, tine courte puis corps long avec chorus.", { depth: 10, ops: [
  { ratio: 1, level: 42, shape: adsr(1, 550, 0, 120) }, { ratio: 1, level: 90, shape: adsr(4, 1600, 12, 500) },
  { ratio: 14, level: 18, shape: adsr(0, 180, 0, 100) }, { ratio: 1, level: 75, shape: adsr(3, 1200, 8, 400) }], chorus: { Rate: 70, Depth: 25, Mix: 24 }, delay: { Mix: 12, Feedback: 18 } });
fm("KEY EP Neon", "Claviers", "Piano electrique plus brillant, tine metallique et chorus stereo.", { depth: 16, feedback: 3, ops: [
  { ratio: 2, level: 46, shape: adsr(0, 700, 5, 160) }, { ratio: 1, level: 90, shape: adsr(2, 1700, 12, 500) },
  { ratio: 7, level: 27, shape: adsr(0, 260, 0, 120) }, { ratio: 1, level: 80, shape: adsr(2, 1300, 8, 450) }], chorus: { Rate: 90, Depth: 32, Mix: 28 }, delay: { Mix: 16, Feedback: 22 } });
fm("KEY Tine Bell", "Claviers", "Cloche FM a deux paires, harmoniques longues et echo ping-pong.", { depth: 13, ops: [
  { ratio: 5, level: 44, shape: adsr(0, 1300, 0, 500) }, { ratio: 1, level: 90, shape: adsr(2, 2000, 0, 900) },
  { ratio: 7, level: 23, shape: adsr(0, 900, 0, 400) }, { ratio: 2, level: 55, shape: adsr(2, 1800, 0, 700) }], delay: { Time: 400, "Time R": 600, Mix: 24, Feedback: 32 } });
fm("KEY Music Box", "Claviers", "Boite a musique FM, attaque fine et decroissance sans sustain.", { depth: 8, pitch: 12, ops: [
  { ratio: 3, level: 26, shape: adsr(0, 200, 0, 100) }, { ratio: 1, level: 90, shape: adsr(1, 1400, 0, 500) },
  { ratio: 5, level: 16, shape: adsr(0, 300, 0, 120) }, { ratio: 2, level: 35, shape: adsr(1, 900, 0, 400) }], delay: { Mix: 18, Feedback: 26 } });
fm("KEY Pluck Glass", "Claviers", "Pluck FM percussif, profondeur qui chute vite et echo a la croche.", { algorithm: 0, depth: 12, ops: [
  { ratio: 2, level: 30, shape: adsr(0, 75, 0, 60) }, { ratio: 3, level: 38, shape: adsr(0, 180, 0, 90) },
  { ratio: 1, level: 35, shape: adsr(0, 220, 0, 100) }, { ratio: 1, level: 90, shape: adsr(1, 700, 0, 250) }], delay: { Sync: 1, Division: 4, "Division R": 3, Mix: 25, Feedback: 30 } });
fm("KEY Organ Rotary", "Claviers", "FM additive utilisee en tirettes : fondamentale, octave, quinte, octave haute.", { algorithm: 7, depth: 0, ops: [
  { ratio: 1, level: 90, shape: adsr(5, 70, 100, 90) }, { ratio: 2, level: 65, shape: adsr(5, 70, 100, 90) },
  { ratio: 3, level: 40, shape: adsr(3, 220, 70, 90) }, { ratio: 4, level: 25, shape: adsr(2, 180, 55, 80) }], disto: { Drive: 15, Mix: 18 }, rotary: { Speed: 0, Depth: 70, Mix: 80 } });

// Six FX, including utility CV and granular instruments.
{
  const p = new Patch("FX Noise Sweep", "FX", "Souffle filtre : ouverture de 1,8 s, amplitude douce et delay.", 1);
  let tail = p.node("Noise", { Level: 58 });
  const filter = p.node("Filter", { Mode: 3, Order: 0, Cutoff: 1000, Reso: 25 });
  tail = p.connect(tail, filter);
  p.env(filter, "Cutoff", 80, adsr(1800, 1600, 80, 2000));
  tail = p.effect(tail, "ADSR", adsr(900, 800, 85, 1800));
  p.finish(tail, { delay: { Time: 450, "Time R": 675, Mix: 30, Feedback: 38 } });
}
{
  const p = new Patch("FX Ring Visitor", "FX", "Deux triangles multiplies, quinte metallique, animation et ping-pong.", 2);
  const a = p.oscillator(osc(2, 0, 0, 80)), b = p.oscillator(osc(2, 7, 11, 80));
  const ring = p.node("Ring Mod", { Mix: 85 }); p.connect(a, ring, 0); p.connect(b, ring, 1);
  p.lfo(b, "Pitch", 8, 10);
  let tail = p.effect(ring, "Filter", { Mode: 1, Order: 0, Cutoff: 6000, Reso: 12 });
  tail = p.effect(tail, "ADSR", adsr(150, 700, 65, 1100));
  p.finish(tail, { chorus: { Rate: 28, Depth: 35, Mix: 25 }, delay: { Mix: 28, Feedback: 38 } });
}
grain("FX Grain Storm", "FX", "Texture de metal fournie, grains denses et dispersion animee.", metalSample, { grain: { Size: 85, Density: 55, Speed: 75, Spray: 80, Level: 65 }, amp: adsr(350, 600, 90, 1100), lfo: { target: "Pitch", rate: 4, depth: 12 }, filter: { Cutoff: 5500 }, delay: { Mix: 30, Feedback: 38 } });
grain("FX Reverse Dust", "FX", "Poussiere rythmee fournie, lecture granulaire inverse, filtre passe-haut et grain sature.", dustSample, { grain: { Size: 220, Density: 18, Speed: -55, Spray: 35, Pitch: -12, Level: 90 }, amp: adsr(600, 800, 80, 1500), filter: { Mode: 2, Cutoff: 120 }, disto: { Drive: 28, Mix: 40 }, chorus: { Rate: 40, Depth: 45, Mix: 25 }, delay: { Time: 550, "Time R": 730, Mix: 32, Feedback: 42 } });
{
  const p = new Patch("FX Radio Steps", "FX", "Random declenche a la croche, Slew et filtre passe-bande : brouillage musical.", 1);
  const source = p.oscillator(osc(digital, 0, 0, 65));
  const filter = p.node("Filter", { Mode: 3, Order: 0, Cutoff: 1500, Reso: 40 });
  p.connect(source, filter);
  const clock = p.node("Clock", { Division: 4 });
  const random = p.node("Random", { Depth: 70, Offset: 0 });
  const slew = p.node("Slew", { Rise: 40, Fall: 70 });
  p.route(clock, random, "Trigger", "Trigger"); p.route(random, slew, "Value", "CV"); p.mod(slew, filter, "Cutoff");
  let tail = p.effect(filter, "ADSR", adsr(80, 300, 85, 500));
  p.finish(tail, { delay: { Time: 250, "Time R": 375, Mix: 28, Feedback: 38 } });
}
{
  const p = new Patch("FX Cosmic Rise", "FX", "Tables granulaires et vocales, montee de hauteur et wavefolder leger.", 2);
  const a = p.oscillator(osc(digital, 0, -7, 65)), b = p.oscillator(osc(vocal, 12, 7, 45));
  const mix = p.node("Mix", { "In 1": 100, "In 2": 100, "In 3": 0 }); p.connect(a, mix, 0); p.connect(b, mix, 1);
  const rise = p.env(a, "Pitch", 80, adsr(2000, 1000, 100, 1800)); p.mod(rise, b, "Pitch");
  let tail = p.effect(mix, "Wavefolder", { Drive: 10, Symmetry: 0, Mix: 25 });
  tail = p.effect(tail, "Filter", { Mode: 1, Order: 0, Cutoff: 5000, Reso: 12 });
  tail = p.effect(tail, "ADSR", adsr(1600, 1000, 100, 2200));
  p.finish(tail, { chorus: { Rate: 30, Depth: 50, Mix: 30 }, delay: { Time: 500, "Time R": 750, Mix: 30, Feedback: 40 } });
}

fm("PERC FM Kick", "Percussions", "Kick FM accorde : jouer autour de C2/MIDI 36, chute de pitch et corps court.", { voices: 1, depth: 5, algorithm: 4, ops: [
  { ratio: 1, level: 30, shape: adsr(0, 35, 0, 40) }, { ratio: 1, level: 100, shape: adsr(0, 500, 0, 80) },
  { ratio: 3, level: 12, shape: adsr(0, 18, 0, 25) }, { ratio: 1, level: 40, shape: adsr(0, 120, 0, 50) }], pitchEnv: { depth: 100, shape: adsr(0, 60, 0, 60) }, disto: { Drive: 18, Mix: 20 } });
fm("PERC FM Metal Hit", "Percussions", "Percussion FM metallique, ratios eleves et echo court.", { depth: 22, feedback: 8, voices: 3, ops: [
  { ratio: 7, level: 60, shape: adsr(0, 180, 0, 90) }, { ratio: 1, level: 90, shape: adsr(0, 500, 0, 180) },
  { ratio: 11, level: 40, shape: adsr(0, 120, 0, 80) }, { ratio: 2, level: 70, shape: adsr(0, 350, 0, 130) }], delay: { Time: 150, "Time R": 225, Mix: 16, Feedback: 22 } });

if (bank.length !== 39) throw new Error("Expected 39 additional presets, got " + bank.length);
for (let i = 0; i < bank.length; i++) {
  const item = bank[i];
  item.file = `${String(i + 2).padStart(2, "0")}-${item.patch.name.toLowerCase().replace(/[^a-z0-9]+/g, "-")}.json`;
  await writeFile(new URL(item.file, root), JSON.stringify(item.patch, null, 2) + "\n");
}
const files = (await readdir(root)).filter(f => /^[a-z0-9-]+\.json$/.test(f));
if (files.length !== 40) throw new Error("Expected exactly 40 bank files, got " + files.length);
const first = { file: "01-va-iron-bass.json", patch: { name: "VA Iron Bass" }, category: "Basses", description: "Premier patch valide : basse agressive, deux pulses et triangle grave, filtre et distorsion." };
const doc = ["# Banque de 40 presets Modulox", "", "Ouvrir **Presets d’usine**, choisir un nom, puis **Envoyer au Modulox**. Pour une copie personnelle sur disque : **Enregistrer dans mon dossier**.",
  "Les formes AKWF et les textures granulaires necessaires sont importees automatiquement dans les bibliotheques du navigateur au chargement du preset, puis transferees par USB avec lui.",
  "Les presets ont un volume général de 85 %. Le bouton Charger tous les presets sur le Modulox dans Presets d’usine envoie toute la banque, avec progression, reprise et conservation des copies personnelles modifiées. Le firmware 1.1 ou suivant accepte jusqu’à 64 patches selon la place disponible.", "", "Basses et leads : trois oscillateurs VA chacun. Les autres familles utilisent librement VA, tables AKWF, FM4, bruit, ring modulation et Granulizer.",
  "Les basses et les solos mono ont un glide legato ; les nouvelles basses gardent leurs enveloppes en legato (Retrigger Off). Jouer les basses autour de MIDI 36–48, les leads/claviers/nappes autour de 60–72.",
  "Les wobble et Radio Steps utilisent l'horloge interne a 120 BPM ; Clock/BPM restent modifiables dans les reglages du patch.", "", "Les intitulés de timbre decrivent l'intention : l'ecoute finale et les ajustements restent a faire sur le Modulox.", "", "| # | Famille | Preset | Intention |", "| --- | --- | --- | --- |"];
for (const [i, item] of [first, ...bank].entries()) doc.push(`| ${i + 1} | ${item.category} | [${item.patch.name}](dist/presets/${item.file}) | ${item.description} |`);
doc.push("", "## Ressources et regeneration", "", "Quatre cycles AKWF de Kristoffer Ekstrand / Adventure Kid, pris dans la bibliotheque deja presente du projet, sont reechantillonnes a 1024 points. Attribution originale : `dist/presets/assets/AKWF-CREDITS.txt`. Le manifeste conserve les chemins sources exacts.",
  "Trois textures C4 originales de trois secondes (choeur synthetique, metal et poussiere pulsee), PCM mono 16 bits/44,1 kHz, sont generees de maniere deterministe. Aucun telechargement ni sample externe n'est requis.",
  "Les identifiants sont les sept premiers octets du SHA-256 du PCM. Le chargeur verifie tailles et empreintes avant de sauvegarder les ressources.",
  "Regeneration : `node editor/build-presets.mjs` depuis la racine. Le premier preset valide n'est pas reecrit.", "");
await writeFile(new URL("PRESETS.md", import.meta.url), doc.join("\n"));
console.log(`Created ${bank.length} presets + original, ${manifest.waves.length} waves, ${manifest.samples.length} textures in ${fileURLToPath(root)}`);
