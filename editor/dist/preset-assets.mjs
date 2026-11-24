import { waveId, waveBytes } from "./waves.mjs";
import { maxSampleBytes } from "./samples.mjs";

// Load only the assets needed by the selected preset. PCM hashes are also
// the identifiers used by the USB protocol, so transfers can reuse them.
export async function loadPresetAssets(patch, { waves, samples, saveWaves, saveSample, fetch: request = fetch }) {
  const waveIds = [...new Set(patch.waves ?? [])].filter(id => !waves.has(id));
  const sampleIds = [...new Set(patch.modules.filter(n => ["Sampler", "Granulizer"].includes(n.type))
    .map(n => n.sample))].filter(id => !samples.has(id));
  if (!waveIds.length && !sampleIds.length) return;
  const response = await request("/presets/assets/index.json");
  if (!response.ok) throw new Error("Bibliothèque des presets indisponible.");
  const manifest = await response.json();
  const read = async (id, kind) => {
    const entry = manifest[kind]?.find(item => item.id === id);
    if (!/^[a-f0-9]{14}$/.test(id) || !entry) throw new Error("Ressource absente du serveur : " + id);
    const response = await request("/presets/assets/" + id + ".pcm");
    if (!response.ok) throw new Error("Ressource indisponible : " + entry.name);
    const pcm = new Uint8Array(await response.arrayBuffer());
    if (kind === "waves" ? pcm.length !== waveBytes : pcm.length < 4 || pcm.length % 2 || pcm.length > maxSampleBytes)
      throw new Error("Taille PCM invalide : " + entry.name);
    if (await waveId(pcm) !== id) throw new Error("Ressource corrompue : " + entry.name);
    return { id, name: entry.name, pcm };
  };
  const newWaves = await Promise.all(waveIds.map(id => read(id, "waves")));
  const newSamples = await Promise.all(sampleIds.map(id => read(id, "samples")));
  if (newWaves.length) await saveWaves(newWaves);
  for (const sample of newSamples) await saveSample(sample);
  for (const wave of newWaves) waves.set(wave.id, wave);
  for (const sample of newSamples) samples.set(sample.id, sample);
}
