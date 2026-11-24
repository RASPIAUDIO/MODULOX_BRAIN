import { normalizePatch, offlineCatalog, patchForDevice, pitchCatalog, validatePatch } from "./patch.mjs?v=15";
import { loadPresetAssets } from "./preset-assets.mjs";
import { uploadPatch } from "./protocol.mjs?v=18";

export async function loadFactoryBank(request = fetch) {
  const response = await request("/presets/index.json", { cache: "no-store" });
  if (!response.ok) throw new Error("Presets d’usine indisponibles.");
  const files = await response.json();
  if (!Array.isArray(files) || !files.length || files.length > 64 || new Set(files).size !== files.length ||
      files.some(file => typeof file !== "string" || !/^[a-z0-9-]+\.json$/.test(file)))
    throw new Error("Liste des presets d’usine invalide.");
  const items = [];
  // Bound parallel requests for the lightweight local Python server.
  for (let offset = 0; offset < files.length; offset += 4) {
    items.push(...await Promise.all(files.slice(offset, offset + 4).map(async file => {
      const response = await request("/presets/" + file, { cache: "no-store" });
      if (!response.ok) throw new Error("Preset indisponible : " + file);
      const patch = normalizePatch(await response.json());
      const error = validatePatch(patch, offlineCatalog);
      if (error) throw new Error(file + " : " + error);
      return { file, patch };
    })));
  }
  return items;
}

async function presetKey(file) {
  const bytes = new TextEncoder().encode("modulox.factory.v1:" + file);
  const hash = new Uint8Array(await crypto.subtle.digest("SHA-256", bytes));
  return "u" + Array.from(hash, b => b.toString(16).padStart(2, "0")).join("").slice(0, 13);
}

function canonical(patch, soundOnly = false) {
  const value = structuredClone(normalizePatch(patch));
  delete value.layout;
  if (soundOnly) {
    delete value.category;
    if (value.settings) {
      delete value.settings.Volume;
      if (!Object.keys(value.settings).length) delete value.settings;
    }
  }
  const sorted = value => Array.isArray(value) ? value.map(sorted) : value && typeof value === "object"
    ? Object.fromEntries(Object.keys(value).sort().map(key => [key, sorted(value[key])])) : value;
  return JSON.stringify(sorted(value));
}

export async function planFactoryUpload(items, entries, deviceCatalog) {
  if (!deviceCatalog?.maxPatches || !deviceCatalog.maxBytes) throw new Error("Connecte un Modulox compatible avant l’envoi.");
  if (deviceCatalog.firmware?.pendingVerify) throw new Error("Attends la confirmation du démarrage du firmware.");
  const catalog = pitchCatalog(deviceCatalog), used = new Set();
  const plan = [];
  for (const item of items) {
    const error = validatePatch(item.patch, catalog);
    if (error) throw new Error(item.patch.name + " : " + error);
    const patch = patchForDevice(structuredClone(item.patch), catalog);
    if (new TextEncoder().encode(JSON.stringify(patch)).length > catalog.maxBytes)
      throw new Error(item.patch.name + " dépasse la taille acceptée par ce firmware.");
    let key = await presetKey(item.file);
    // A customized copy keeps its slot, even if originally created by this button.
    // A stable alternate slot receives the fresh factory sound instead.
    for (let copy = 1; entries.some(entry => entry.file === key + ".json" && canonical(entry.patch, true) !== canonical(item.patch, true)); ++copy) {
      if (copy > entries.length + 1) throw new Error("Impossible de réserver un emplacement pour " + item.patch.name);
      key = await presetKey(item.file + "#" + copy);
    }
    // First reuse this button's stable slot. Adopt older manually uploaded copies
    // only if their sound is untouched (volume, category and canvas layout aside).
    const previous = entries.find(entry => entry.file === key + ".json") ?? entries.find(entry =>
      /^u[0-9a-f]{13}\.json$/.test(entry.file) && !used.has(entry.file) && canonical(entry.patch, true) === canonical(item.patch, true));
    const target = previous ? previous.file.slice(0, -5) : key;
    const file = target + ".json";
    if (used.has(file)) throw new Error("Deux presets ciblent le même emplacement : " + item.patch.name);
    used.add(file);
    const unchanged = Boolean(previous && canonical(previous.patch) === canonical(item.patch));
    plan.push({ ...item, patch, key: target, targetFile: file, action: !previous ? "add" : unchanged ? "skip" : "update" });
  }
  const added = plan.filter(item => item.action === "add").length;
  const updated = plan.filter(item => item.action === "update").length;
  if (entries.length + added > catalog.maxPatches) throw new Error(
    `Place insuffisante : ${added} nouveaux emplacements nécessaires, ${Math.max(0, catalog.maxPatches - entries.length)} disponibles. ` +
    (catalog.maxPatches < items.length ? "Le chargement complet nécessite le firmware 1.1 ou suivant (64 patches)." : "Libère des emplacements avant de recommencer."));
  return { items: plan, added, updated, unchanged: plan.length - added - updated };
}

export async function uploadFactoryBank(plan, { port, catalog, libraries, signal, progress = () => {}, onSaved = () => {},
  upload = uploadPatch, loadAssets = loadPresetAssets }) {
  let saved = 0, done = 0;
  try {
    // All dependencies are loaded and checked before the first USB patch write.
    for (const item of plan.items) {
      if (signal?.aborted) return { saved, done, cancelled: true };
      if (item.action !== "skip") {
        progress({ phase: "prepare", item, done, total: plan.items.length });
        await loadAssets(item.patch, libraries);
      }
    }
    for (const item of plan.items) {
      if (signal?.aborted) return { saved, done, cancelled: true };
      progress({ phase: "upload", item, done, total: plan.items.length });
      if (item.action !== "skip") {
        const id = await upload(port, item.key, item.patch, catalog.maxBytes, catalog.maxPatches,
          libraries.waves, undefined, libraries.samples, undefined, catalog.sampleNames);
        ++saved;
        onSaved({ id, file: item.targetFile, key: item.targetFile, patch: structuredClone(item.patch) });
      }
      ++done;
      progress({ phase: "complete", item, done, total: plan.items.length });
    }
    return { saved, done, cancelled: false };
  } catch (cause) {
    throw new Error(`Envoi interrompu : ${saved} patch(s) enregistré(s), ${done}/${plan.items.length} traités. ${cause.message} ` +
      "Tu peux relancer : les presets déjà à jour seront conservés.", { cause });
  }
}
