import { normalizePatch, validatePatch, offlineCatalog, sampleModuleTypes } from "./patch.mjs";
import { waveId, waveBytes } from "./waves.mjs";
import { maxSampleBytes } from "./samples.mjs";

export const maxPatchFileBytes = 8 * 1024 * 1024;
const format = "modulox.patch";
const idsFor = patch => ({ waves: patch.waves ?? [], samples: [...new Set(patch.modules
  .filter(node => sampleModuleTypes.includes(node.type) && node.sample).map(node => node.sample))] });
function checkedPatch(value) {
  try {
    const patch = normalizePatch(value);
    const error = validatePatch(patch, offlineCatalog, { allowIncomplete: true });
    if (error) throw new Error(error);
    return patch;
  } catch (error) { throw new Error("Patch invalide : " + error.message); }
}
function base64(bytes) {
  let binary = "";
  for (let i = 0; i < bytes.length; i += 8192) binary += String.fromCharCode(...bytes.subarray(i, i + 8192));
  return btoa(binary);
}
async function checkedResource(resource, kind) {
  const pcm = resource?.pcm;
  if (!(pcm instanceof Uint8Array) || !/^[a-f0-9]{14}$/.test(resource.id) ||
      (kind === "waves" ? pcm.length !== waveBytes : pcm.length < 4 || pcm.length % 2 || pcm.length > maxSampleBytes) ||
      await waveId(pcm) !== resource.id) throw new Error("Ressource absente ou corrompue : " + (resource?.name ?? resource?.id ?? kind));
  return { id: resource.id, name: String(resource.name ?? resource.id).slice(0, 60), pcm };
}
export async function packPatch(value, libraries) {
  const patch = checkedPatch(value), ids = idsFor(patch);
  const bundle = { format, version: 1, patch, waves: [], samples: [] };
  for (const kind of ["waves", "samples"]) for (const id of ids[kind]) {
    const resource = await checkedResource(libraries[kind].get(id), kind);
    bundle[kind].push({ ...resource, pcm: base64(resource.pcm) });
  }
  const text = JSON.stringify(bundle);
  if (new TextEncoder().encode(text).length > maxPatchFileBytes) throw new Error("Fichier de patch trop volumineux (8 Mio maximum).");
  return text;
}
export async function unpackPatch(text, libraries = { waves: new Map(), samples: new Map() }) {
  if (typeof text !== "string" || new TextEncoder().encode(text).length > maxPatchFileBytes) throw new Error("Fichier de patch trop volumineux (8 Mio maximum).");
  let bundle;
  try { bundle = JSON.parse(text); } catch { throw new Error("Ce fichier n’est pas un JSON valide."); }
  const portable = bundle?.format === format;
  if (bundle?.format !== undefined && (!portable || bundle.version !== 1)) throw new Error("Version de fichier incompatible.");
  const patch = checkedPatch(portable ? bundle.patch : bundle), ids = idsFor(patch);
  const result = { patch, waves: [], samples: [] };
  for (const kind of ["waves", "samples"]) {
    if (portable && (!Array.isArray(bundle[kind]) || bundle[kind].length !== ids[kind].length)) throw new Error("Ressources du fichier incomplètes : " + kind);
    const seen = new Set();
    for (const id of ids[kind]) {
      let resource;
      if (portable) {
        const encoded = bundle[kind].find(item => item?.id === id);
        if (!encoded || seen.has(id) || typeof encoded.pcm !== "string" || encoded.pcm.length % 4 ||
            !/^[A-Za-z0-9+/]*={0,2}$/.test(encoded.pcm)) throw new Error("Ressource manquante ou encodage invalide : " + id);
        const limit = kind === "waves" ? waveBytes : maxSampleBytes;
        if (encoded.pcm.length > Math.ceil(limit / 3) * 4) throw new Error("Ressource trop volumineuse : " + id);
        resource = { ...encoded, pcm: Uint8Array.from(atob(encoded.pcm), char => char.charCodeAt(0)) };
      } else resource = libraries[kind].get(id);
      result[kind].push(await checkedResource(resource, kind)); seen.add(id);
    }
  }
  return result;
}
export async function readPatchFile(file, libraries) {
  if (file.size > maxPatchFileBytes) throw new Error("Fichier de patch trop volumineux (8 Mio maximum).");
  return unpackPatch(await file.text(), libraries);
}
export async function writePatchFile(directory, contents, name) {
  const slug = name.normalize("NFD").replace(/\p{Mark}/gu, "").replace(/[^a-zA-Z0-9-]+/g, "-").slice(0, 60) || "patch";
  const filename = slug + "-" + crypto.randomUUID() + ".modulox.json";
  const handle = await directory.getFileHandle(filename, { create: true });
  let writer;
  try { writer = await handle.createWritable(); await writer.write(contents); await writer.close(); }
  catch (error) {
    await writer?.abort().catch(() => {});
    await directory.removeEntry(filename).catch(() => {});
    throw error;
  }
  return filename;
}
// Only the selected directory handle is remembered here. Patch files live in
// the user's actual directory, not in this origin's private filesystem.
export async function folderPreference(handle) {
  const db = await new Promise((resolve, reject) => {
    const request = indexedDB.open("modulox-folder", 1);
    request.onupgradeneeded = () => request.result.createObjectStore("settings");
    request.onsuccess = () => resolve(request.result); request.onerror = () => reject(request.error);
  });
  try {
    return await new Promise((resolve, reject) => {
      const tx = db.transaction("settings", handle ? "readwrite" : "readonly");
      const request = handle ? tx.objectStore("settings").put(handle, "folder") : tx.objectStore("settings").get("folder");
      tx.oncomplete = () => resolve(request.result); tx.onabort = () => reject(tx.error);
    });
  } finally { db.close(); }
}
