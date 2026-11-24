import { waveId } from "./waves.mjs";

export const sampleRate = 44100;
export const maxSampleBytes = 1 << 20;
export const sampleChunkBytes = 4096;

export async function readSample(file) {
  const context = new OfflineAudioContext(1, 1, sampleRate);
  const audio = await context.decodeAudioData(await file.arrayBuffer());
  if (audio.length < 2 || audio.length * 2 > maxSampleBytes) {
    throw new Error("Durée maximale : " + (maxSampleBytes / (2 * sampleRate)).toFixed(1) + " s.");
  }
  const channels = Array.from({ length: audio.numberOfChannels }, (_, index) => audio.getChannelData(index));
  const pcm = new Uint8Array(audio.length * 2);
  const view = new DataView(pcm.buffer);
  for (let frame = 0; frame < audio.length; frame++) {
    let value = 0;
    for (const channel of channels) value += channel[frame];
    const sample = Math.round(value * 32768 / channels.length);
    view.setInt16(2 * frame, Math.max(-32768, Math.min(32767, sample)), true);
  }
  return { id: await waveId(pcm), name: file.name.replace(/\.wav$/i, "").slice(0, 60), pcm };
}

async function storage(mode, run) {
  const db = await new Promise((resolve, reject) => {
    const request = indexedDB.open("modulox-samples", 1);
    request.onupgradeneeded = () => request.result.createObjectStore("samples", { keyPath: "id" });
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  try {
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction("samples", mode);
      const request = run(transaction.objectStore("samples"));
      transaction.oncomplete = () => resolve(request?.result);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

export const readSamples = () => storage("readonly", (store) => store.getAll());
export const saveSample = (sample) => storage("readwrite", (store) => store.put(sample));
export const deleteSample = (id) => storage("readwrite", (store) => store.delete(id));

export function sampleWav(sample) {
  const bytes = new Uint8Array(44 + sample.pcm.byteLength);
  const view = new DataView(bytes.buffer);
  const text = (offset, value) => { for (let i = 0; i < value.length; i++) bytes[offset + i] = value.charCodeAt(i); };
  text(0, "RIFF"); view.setUint32(4, bytes.length - 8, true); text(8, "WAVE"); text(12, "fmt ");
  view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
  view.setUint32(24, sampleRate, true); view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true); view.setUint16(34, 16, true);
  text(36, "data"); view.setUint32(40, sample.pcm.byteLength, true); bytes.set(sample.pcm, 44);
  return bytes;
}

export function drawSample(canvas, sample) {
  const ctx = canvas.getContext("2d");
  const { width, height } = canvas;
  const center = height / 2;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = "#4b5060";
  ctx.fillRect(0, center, width, 1);
  if (!sample) return;
  const preview = sample.preview;
  const pcm = sample.pcm ? new DataView(sample.pcm.buffer, sample.pcm.byteOffset, sample.pcm.byteLength) : undefined;
  if (!pcm && !preview) return;
  const frames = preview ? preview.low.length : pcm.byteLength / 2;
  ctx.fillStyle = "#b8acff";
  for (let x = 0; x < width; x++) {
    const start = Math.floor(x * frames / width);
    const end = Math.max(start + 1, Math.floor((x + 1) * frames / width));
    let low = 0;
    let high = 0;
    for (let i = start; i < end; i++) {
      low = Math.min(low, (preview ? preview.low[i] : pcm.getInt16(2 * i, true)) / 32768);
      high = Math.max(high, (preview ? preview.high[i] : pcm.getInt16(2 * i, true)) / 32768);
    }
    ctx.fillRect(x, center - high * center, 1, Math.max(1, (high - low) * center));
  }
}
