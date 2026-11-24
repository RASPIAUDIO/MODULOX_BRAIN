import { tr } from "./i18n.mjs";
import { decodeWave, encodeWave, waveBytes, waveNameBytes } from "./waves.mjs";
import { waveId } from "./waves.mjs";
import { maxSampleBytes, sampleChunkBytes } from "./samples.mjs";
import { sampleModuleTypes } from "./patch.mjs?v=9";

const decoder = new TextDecoder("utf-8", { fatal: true });
const encoder = new TextEncoder();

class DeviceError extends Error {
  constructor(code) {
    const messages = {
      invalid: tr("Le Modulox a refusé le patch. Vérifie son câblage et ses paramètres."),
      full: tr("La banque de patches du Modulox est pleine (emplacements ou espace flash). Supprime un patch pour libérer de la place."),
      storage: tr("Le Modulox n’a pas pu écrire le patch en flash."),
      timeout: tr("Le transfert du patch a expiré."),
      "wave-storage": tr("Le Modulox n’a pas pu écrire la forme d’onde en flash."),
      "wave-full": tr("La flash des formes d’onde est pleine. Les formes utilisées par les patches enregistrés sont conservées."),
      "wave-invalid": tr("Le Modulox a refusé cette forme d’onde."),
      "wave-missing": tr("Cette forme d’onde est absente du Modulox."),
      "sample-storage": tr("Le Modulox n’a pas pu écrire le sample en flash. Vérifie l’espace disponible."),
      "sample-missing": tr("Ce sample est absent du Modulox."),
      "sample-invalid": tr("Le Modulox a refusé ce sample."),
      "sample-in-use": tr("Ce sample est utilisé par un patch sauvegardé ou par le patch chargé sur le Modulox. Remplace-le dans ce patch avant de le supprimer."),
      "sample-busy": tr("Un enregistrement ou un transfert de sample est en cours. Réessaie après sa sauvegarde."),
      "recording-busy": tr("Un enregistrement est en cours sur le Modulox. Attends la fin de sa sauvegarde."),
      "screen-busy": tr("Les commandes du Modulox sont occupées. Réessaie dans un instant."),
      "firmware-busy": tr("Une mise à jour ou une vérification de démarrage est en cours."),
      "firmware-layout": tr("Le Modulox nécessite d’abord la migration vers le partitionnement A/B."),
      "firmware-invalid": tr("Le firmware a été refusé : image ou transfert invalide."),
      "firmware-incompatible": tr("Ce firmware ne correspond pas à ce Modulox."),
      "firmware-storage": tr("L’écriture du firmware a échoué. Le firmware actif est conservé."),
      "firmware-incomplete": tr("Le transfert du firmware est incomplet."),
      "firmware-hash": tr("Le contrôle SHA-256 du firmware a échoué."),
      "firmware-no-rollback": tr("Aucun firmware précédent valide n’est disponible."),
    };
    super(messages[code] ?? tr("Commande refusée par le Modulox."));
    this.code = code;
  }
}

async function exchange(port, run, timeoutMs = 8000) {
  let reader;
  let writer;
  let bytes = new Uint8Array();
  let timedOut = false;
  let timer;

  function armTimeout() {
    clearTimeout(timer);
    timer = setTimeout(() => {
      timedOut = true;
      reader.cancel().catch(() => {});
      writer.abort().catch(() => {});
    }, timeoutMs);
  }

  async function fill() {
    const result = await reader.read();
    if (result.done) throw new Error(tr("La connexion USB a été interrompue."));
    const next = new Uint8Array(bytes.length + result.value.length);
    next.set(bytes);
    next.set(result.value, bytes.length);
    bytes = next;
    armTimeout();
  }

  async function take(size) {
    while (bytes.length < size) await fill();
    const value = bytes.subarray(0, size);
    bytes = bytes.subarray(size);
    return value;
  }

  async function line() {
    while (!bytes.includes(10) && bytes.length < 128) await fill();
    const end = bytes.indexOf(10);
    if (end < 0 || end >= 128) throw new Error(tr("Réponse série invalide."));
    const value = decoder.decode((await take(end + 1)).subarray(0, end));
    if (value === "ERR" || value.startsWith("ERR ")) throw new DeviceError(value.slice(4));
    return value;
  }

  async function command(value) {
    armTimeout();
    await writer.write(encoder.encode(value + "\n"));
  }

  async function number(max) {
    const match = /^OK (\d+)$/.exec(await line());
    if (!match || Number(match[1]) > max) throw new Error(tr("Réponse inattendue du Modulox."));
    return Number(match[1]);
  }

  async function json(max) {
    const size = await number(max);
    return JSON.parse(decoder.decode(await take(size)));
  }

  try {
    reader = port.readable.getReader();
    writer = port.writable.getWriter();
    armTimeout();
    return await run({ command, number, line, json, take, write: (data) => writer.write(data) });
  } catch (error) {
    if (timedOut) throw new Error(tr("Le Modulox ne répond pas."));
    throw error;
  } finally {
    clearTimeout(timer);
    reader?.releaseLock();
    writer?.releaseLock();
  }
}

export const fetchFirmwareInfo = port => exchange(port, async ({ command, json }) => {
  await command("FWINFO"); return json(2048);
});

export function fetchScreenBands(port, revision, firstBand) {
  return exchange(port, async ({ command, number, take }) => {
    await command(`SCREEN ${revision} ${firstBand}`);
    return take(await number(20480));
  });
}

export function sendScreenKey(port, key) {
  return exchange(port, async ({ command, number }) => {
    await command(`UIKEY ${key}`);
    await number(0);
  });
}
export const rollbackFirmware = port => exchange(port, async ({ command, number }) => {
  await command("FWROLLBACK"); await number(0);
});
export async function uploadFirmware(port, image, sha256, progress, signal) {
  if (!(image instanceof Uint8Array) || image.length < 288 || image.length > 0x180000 || !/^[a-f0-9]{64}$/.test(sha256)) throw new Error(tr("Firmware invalide."));
  return exchange(port, async ({ command, line, number, write }) => {
    let begun = false, committed = false;
    try {
      if (signal?.aborted) throw new DOMException(tr("Mise à jour annulée."), "AbortError");
      await command(`FWBEGIN ${image.length} ${sha256}`); await number(0); begun = true;
      for (let offset = 0; offset < image.length; offset += 4096) {
        if (signal?.aborted) throw new DOMException(tr("Mise à jour annulée."), "AbortError");
        const chunk = image.subarray(offset, offset + 4096);
        await command(`FWCHUNK ${offset} ${chunk.length}`);
        if (await line() !== "READY") throw new Error(tr("Le Modulox n’est pas prêt à recevoir le firmware."));
        await write(chunk); await number(0); progress?.(offset + chunk.length, image.length);
      }
      if (signal?.aborted) throw new DOMException(tr("Mise à jour annulée."), "AbortError");
      await command("FWEND"); await number(0); committed = true;
    } catch (error) {
      if (begun && !committed) try { await command("FWABORT"); await number(0); } catch { }
      throw error;
    }
  }, 30000);
}

export async function fetchPatches(port) {
  return exchange(port, async ({ command, number, line, json }) => {
    let catalog = null;
    await command("CATALOG");
    try {
      catalog = await json(20480);
    } catch (error) {
      if (!(error instanceof DeviceError) || error.code !== "") throw error;
    }
    await command("LIST");
    const count = await number(catalog?.maxPatches ?? 16);
    const entries = [];
    for (let i = 0; i < count; i++) {
      const match = /^(\d+) ([\w.-]+\.json)$/.exec(await line());
      if (!match || Number(match[1]) !== i) throw new Error(tr("Liste de patches invalide."));
      entries.push({ id: i, file: match[2] });
    }
    for (const entry of entries) {
      await command("GET " + entry.id);
      entry.patch = await json(catalog?.maxBytes ?? 2048);
    }
    return { catalog, entries };
  });
}

async function uploadWaves(session, ids, library, progress) {
  const { command, line, number, write } = session;
  const missing = [];
  for (const id of new Set(ids)) {
    await command("WHAS " + id);
    if (await number(1)) continue;
    const wave = library.get(id);
    if (!wave) throw new Error(tr("Forme d’onde absente. Réimporte le WAV correspondant à {0}.", id));
    missing.push(wave);
  }
  for (const [index, wave] of missing.entries()) {
    progress?.(index + 1, missing.length);
    await command("WPUT " + wave.id);
    if (await line() !== "READY") throw new Error(tr("Le Modulox n’est pas prêt à recevoir la forme d’onde."));
    await write(encodeWave(wave));
    await number(0);
  }
}

export async function fetchWave(port, id) {
  return exchange(port, async ({ command, number, take }) => {
    await command("WGET " + id);
    const size = await number(waveNameBytes + waveBytes);
    return decodeWave(id, await take(size));
  });
}

async function sendSample(session, id, sample, progress, name) {
  const { command, line, number, write } = session;
  await command("SHAS " + id);
  if (!await number(1)) {
    if (!sample) throw new Error(tr("Réimporte le WAV associé au sample {0}.", id));
    await command("SBEGIN " + id + " " + sample.pcm.length);
    await number(0);
    for (let offset = 0; offset < sample.pcm.length; offset += sampleChunkBytes) {
      const chunk = sample.pcm.subarray(offset, offset + sampleChunkBytes);
      await command("SCHUNK " + chunk.length);
      if (await line() !== "READY") throw new Error(tr("Transfert de sample interrompu."));
      await write(chunk);
      await number(0);
      progress?.(offset + chunk.length, sample.pcm.length);
    }
    await command("SCOMMIT");
    await number(0);
  }
  if (name) {
    const label = name.normalize("NFD").replace(/\p{Mark}/gu, "").replace(/[^\x20-\x7e]/g, "_").slice(0, 31);
    if (!label) return;
    await command("SNAME " + id + " " + label.length);
    if (await line() !== "READY") throw new Error(tr("Transfert du nom du sample interrompu."));
    await write(encoder.encode(label));
    await number(0);
  }
}

export const uploadSample = (port, sample, progress, sampleNames) =>
  exchange(port, (session) => sendSample(session, sample.id, sample, progress, sampleNames ? sample.name : undefined));

export async function fetchSample(port, id, name) {
  return exchange(port, async ({ command, number, take }) => {
    const pcm = new Uint8Array(maxSampleBytes);
    let offset = 0;
    for (;;) {
      await command("SGET " + id + " " + offset);
      const count = await number(sampleChunkBytes);
      if (!count) break;
      if (offset + count > pcm.length) throw new Error(tr("Sample trop volumineux."));
      pcm.set(await take(count), offset);
      offset += count;
    }
    const bytes = pcm.slice(0, offset);
    if (offset < 4 || offset % 2 || await waveId(bytes) !== id) throw new Error(tr("Sample USB invalide."));
    return { id, name, pcm: bytes };
  });
}

export async function fetchSampleLibrary(port) {
  return exchange(port, async ({ command, json }) => {
    const samples = new Map();
    let offset = 0;
    for (let page = 0; page < 256; page++) {
      await command("SLIST " + offset);
      const result = await json(8192);
      if (!Array.isArray(result.items) || result.items.length > 32) throw new Error(tr("Liste de samples invalide."));
      for (const sample of result.items) {
        if (!/^[0-9a-f]{14}$/.test(sample.id) || typeof sample.name !== "string" ||
            !Number.isInteger(sample.bytes) || sample.bytes < 4 || sample.bytes > maxSampleBytes || sample.bytes % 2)
          throw new Error(tr("Sample du catalogue invalide."));
        samples.set(sample.id, sample);
      }
      if (result.next === null) return [...samples.values()];
      if (!Number.isInteger(result.next) || result.next !== offset + result.items.length || result.next <= offset)
        throw new Error(tr("Pagination des samples invalide."));
      offset = result.next;
    }
    throw new Error(tr("Catalogue de samples trop volumineux."));
  });
}

export async function fetchSamplePreview(port, id) {
  if (!/^[0-9a-f]{14}$/.test(id)) throw new Error(tr("Identifiant de sample invalide."));
  return exchange(port, async ({ command, number, take }) => {
    await command("SPREVIEW " + id);
    const size = await number(1028);
    if (size !== 1028) throw new Error(tr("Aperçu de sample invalide."));
    const bytes = await take(size);
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const frames = view.getUint32(0, true);
    if (frames < 2 || frames * 2 > maxSampleBytes) throw new Error(tr("Durée de sample invalide."));
    const low = new Int16Array(256), high = new Int16Array(256);
    for (let i = 0; i < 256; i++) {
      low[i] = view.getInt16(4 + 4 * i, true); high[i] = view.getInt16(6 + 4 * i, true);
      if (low[i] > 0 || high[i] < 0) throw new Error(tr("Courbe de sample invalide."));
    }
    return { frames, low, high };
  });
}

export async function deleteDeviceSample(port, id) {
  if (!/^[0-9a-f]{14}$/.test(id)) throw new Error(tr("Identifiant de sample invalide."));
  return exchange(port, async ({ command, number }) => {
    await command("SDELETE " + id);
    return number(0);
  });
}

export async function renameSample(port, id, name) {
  const label = name.normalize("NFD").replace(/\p{Mark}/gu, "").replace(/[^\x20-\x7e]/g, "_").trim().slice(0, 31);
  if (!/^[0-9a-f]{14}$/.test(id) || !label) throw new Error(tr("Choisis un nom de sample valide."));
  return exchange(port, async ({ command, line, write, number }) => {
    await command("SNAME " + id + " " + label.length);
    if (await line() !== "READY") throw new Error(tr("Le Modulox n’est pas prêt à recevoir le nom."));
    await write(encoder.encode(label));
    await number(0);
    return label;
  });
}

export async function uploadPatch(port, key, patch, maxBytes, maxPatches, library, progress, samples, sampleProgress, sampleNames) {
  const payload = encoder.encode(JSON.stringify(patch));
  if (!payload.length || payload.length > maxBytes) throw new Error(tr("Le patch dépasse {0} octets.", maxBytes));
  return exchange(port, async (session) => {
    const { command, line, number, write } = session;
    try {
      await uploadWaves(session, patch.waves ?? [], library, progress);
    } catch (error) {
      if (error.code !== "wave-full") throw error;
      await command("WGC");
      await number(0);
      await uploadWaves(session, patch.waves, library, progress);
    }
    const selectedSamples = new Map(patch.modules.filter((node) => sampleModuleTypes.includes(node.type))
      .map((node) => [node.sample, node.sampleName]));
    for (const [id, name] of selectedSamples) {
      const sample = samples?.get(id);
      await sendSample(session, id, sample, sampleProgress, sampleNames ? name ?? sample?.name : undefined);
    }
    await command("PUT " + key + " " + payload.length);
    if (await line() !== "READY") throw new Error(tr("Le Modulox n’est pas prêt à recevoir le patch."));
    await write(payload);
    return number(maxPatches - 1);
  });
}

export async function deletePatch(port, file) {
  return exchange(port, async ({ command, number }) => {
    await command("DELETE " + file);
    return number(0);
  });
}
