export const waveSamples = 1024;
export const maxPatchWaves = 125;
export const builtinWaves = ["Sine", "Square", "Triangle"];
export const waveBytes = waveSamples * 2;
export const waveNameBytes = 32;
const encoder = new TextEncoder();
const decoder = new TextDecoder();

function interpolateCycle(pcm, frames, position) {
  const index = Math.floor(position);
  const first = pcm.getInt16(index * 2, true);
  const next = pcm.getInt16(((index + 1) % frames) * 2, true);
  return first + (next - first) * (position - index);
}

function resampleCycle(source) {
  if (source.byteLength === waveBytes) {
    return new Uint8Array(source.buffer, source.byteOffset, waveBytes).slice();
  }
  const frames = source.byteLength / 2;
  const target = new DataView(new ArrayBuffer(waveBytes));
  for (let i = 0; i < waveSamples; i++) {
    const value = interpolateCycle(source, frames, i * frames / waveSamples);
    target.setInt16(i * 2, Math.round(value), true);
  }
  return new Uint8Array(target.buffer);
}

export async function readWave(file) {
  const buffer = await file.arrayBuffer();
  const view = new DataView(buffer);
  const tag = (offset) => decoder.decode(new Uint8Array(buffer, offset, 4));
  const invalid = () => new Error(file.name + " · attendu un cycle WAV mono PCM 16 bits.");
  if (buffer.byteLength < 12 || tag(0) !== "RIFF" || tag(8) !== "WAVE") throw invalid();
  const end = 8 + view.getUint32(4, true);
  if (end > buffer.byteLength) throw invalid();
  let format;
  let samples;
  for (let offset = 12; offset + 8 <= end;) {
    const size = view.getUint32(offset + 4, true);
    const body = offset + 8;
    if (body + size > end) throw invalid();
    switch (tag(offset)) {
      case "fmt ":
        if (size < 16) throw invalid();
        format = view.getUint16(body, true) === 1 && view.getUint16(body + 2, true) === 1 &&
          view.getUint16(body + 12, true) === 2 && view.getUint16(body + 14, true) === 16;
        break;
      case "data":
        if (size === 0 || size % 2 !== 0) throw invalid();
        samples = new DataView(buffer, body, size);
        break;
    }
    offset = body + size + (size & 1);
  }
  if (!format || !samples) throw invalid();
  const pcm = resampleCycle(samples);
  return { id: await waveId(pcm), name: file.name.replace(/\.wav$/i, ""), pcm };
}

export async function waveId(pcm) {
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", pcm));
  return Array.from(hash.subarray(0, 7), (byte) => byte.toString(16).padStart(2, "0")).join("");
}

export function encodeWave(wave) {
  const bytes = new Uint8Array(waveNameBytes + waveBytes);
  const name = wave.name.normalize("NFD").replace(/\p{Mark}/gu, "").replace(/[^\x20-\x7e]/g, "_");
  bytes.set(encoder.encode(name.slice(0, waveNameBytes - 1)));
  bytes.set(wave.pcm, waveNameBytes);
  return bytes;
}

export async function decodeWave(id, bytes) {
  const pcm = bytes.slice(waveNameBytes);
  const name = decoder.decode(bytes.subarray(0, waveNameBytes)).split("\0")[0];
  if (pcm.length !== waveBytes || await waveId(pcm) !== id) {
    throw new Error("Forme d’onde USB invalide.");
  }
  return { id, name: name || id, pcm };
}

const openLibrary = () => new Promise((resolve, reject) => {
  const request = indexedDB.open("modulox-waves", 1);
  request.onupgradeneeded = () => request.result.createObjectStore("waves", { keyPath: "id" });
  request.onsuccess = () => resolve(request.result);
  request.onerror = () => reject(request.error);
});

async function libraryOperation(mode, run) {
  const db = await openLibrary();
  try {
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction("waves", mode);
      const result = run(transaction.objectStore("waves"));
      transaction.oncomplete = () => resolve(result?.result);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

export const readLibrary = () => libraryOperation("readonly", (store) => store.getAll());
export const deleteWave = (id) => libraryOperation("readwrite", (store) => store.delete(id));
export const saveWaves = (waves) => libraryOperation("readwrite", (store) => {
  for (const wave of waves) store.put(wave);
});

export function drawWave(canvas, wave, builtin, pwm = 50) {
  const ctx = canvas.getContext("2d");
  const { width, height } = canvas;
  ctx.clearRect(0, 0, width, height);
  const center = height / 2;
  ctx.strokeStyle = "#4b5060";
  ctx.beginPath();
  ctx.moveTo(0, center);
  ctx.lineTo(width, center);
  ctx.stroke();
  if (!wave && builtin === undefined) return;
  const samples = wave && new DataView(wave.pcm.buffer, wave.pcm.byteOffset, waveBytes);
  ctx.strokeStyle = "#b8acff";
  ctx.lineWidth = 2;
  ctx.beginPath();
  for (let x = 0; x < width; x++) {
    const phase = x / width;
    let value;
    if (samples) {
      value = interpolateCycle(samples, waveSamples, phase * waveSamples) / 32768;
    } else if (builtin === 1) {
      value = phase < pwm / 100 ? 1 : -1;
    } else if (builtin === 2) {
      value = 1 - 4 * Math.abs(((phase + 0.25) % 1) - 0.5);
    } else if (builtin === 2) {
      value = 1 - 4 * Math.abs(phase - 0.5);
    } else {
      value = Math.sin(2 * Math.PI * phase);
    }
    ctx.lineTo(x, center - value * (center - 6));
  }
  ctx.stroke();
}
