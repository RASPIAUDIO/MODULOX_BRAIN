import { initializeLanguage, tr } from "./i18n.mjs";
import { deleteDeviceSample, deletePatch, fetchPatches, fetchSample, fetchSampleLibrary, fetchSamplePreview, fetchWave, renameSample, uploadPatch, uploadSample, fetchFirmwareInfo, uploadFirmware, rollbackFirmware } from "./protocol.mjs?v=18";
import { builtinWaves, deleteWave, drawWave, maxPatchWaves, readLibrary, readWave, saveWaves } from "./waves.mjs?v=4";
import {
  connectionError, firstOscillator, fm4Topologies, modulationError, moduleSpec, nodeSpec, normalizePatch, offlineCatalog, outputModule,
  patchForDevice, pitchCatalog, playSettings, routingError, sampleModuleTypes, timingModuleTypes, usesVaExpansion,
  stereoModuleTypes, usesFm4, vaModuleTypes, validatePatch, patchCategories,
} from "./patch.mjs?v=15";
import { deleteSample, drawSample, readSample, readSamples, sampleRate, sampleWav, saveSample } from "./samples.mjs?v=7";
import { loadPresetAssets } from "./preset-assets.mjs";
import { loadFactoryBank, planFactoryUpload, uploadFactoryBank } from "./factory-bank.mjs";
import { packPatch, readPatchFile, writePatchFile, folderPreference } from "./patch-files.mjs";
import { compareVersions, fetchFirmwareRelease, fetchFirmwareImage, readFirmwareFile, validateFirmware } from "./firmware.mjs";
import { boardForModel, defaultBoard } from "./hardware.mjs";
import { recoverFirmware } from "./firmware-recovery.mjs";
import { factoryConsentValid, prepareFactoryInstaller } from "./factory-install.mjs";
import { initializeMidiKeyboard } from "./midi-keyboard.mjs";

await initializeLanguage();

const $ = (id) => document.getElementById(id);
const midiKeyboard = initializeMidiKeyboard($("midi-keyboard"));
const graph = $("graph");
const viewport = $("viewport");
const canvas = $("wires");
const draftStorage = "modulox.drafts.v1";
const nodeWidth = 132;
const nodeHeight = (type) => type === "Granulizer" ? 132 :
  Math.max(52, 14 * ((moduleSpec(catalog,type)?.modulationInputs?.length ?? 0) + 1));
const margin = 24;
let drafts = [];
let savedPatches = [];
let patchFolder;
const folderReady = folderPreference().then(handle => { patchFolder = handle; }).catch(() => {});
let entries = [];
let examples = [];
let selection;
let selectedNode;
let cableSource;
let cableMidiPort;
let cableEventPort;
let catalog = offlineCatalog;
let port;
let busy = "";
let hasRead = false;
let canUpload = false;
let deviceCatalog;
let currentFirmware, firmwareRelease, firmwareImage, expectedFirmware, firmwareAbort;
let firmwareManual = false;
let factoryAbort;
let factoryInstallRelease, factoryInstallSession;
let factoryInstallWorking = false, factoryInstallWriting = false, factoryInstallGeneration = 0;
let storageError = "";
let drag;
let positions = new Map();
const waveLibrary = new Map();
const sampleLibrary = new Map();
const remoteSamples = new Map();
const samplePreviewCache = new Map();
const samplePreviewFailures = new Set();
const samplesReady = readSamples().then((samples) => {
  for (const sample of samples) sampleLibrary.set(sample.id, sample);
}).catch(() => {
  $("sample-message").textContent = tr("La bibliothèque locale n’a pas pu être lue.");
});
let selectedSample;
let audioContext;
let samplePlayback;
let screen = "patches";
let editorView = "graph";
let samplesForPatch = false;
const waveLibraryReady = readLibrary().then((waves) => {
  for (const wave of waves) waveLibrary.set(wave.id, wave);
}).catch(() => {
  $("wave-message").textContent = tr("La bibliothèque locale n’a pas pu être lue.");
});
let previewId;

function waveLabel(id) {
  return waveLibrary.get(id)?.name ?? id;
}

function maxWaves() {
  return catalog.maxWaves ?? maxPatchWaves;
}

function patchWaveIds() {
  return samplesForPatch ? activeDraft()?.patch.waves ?? [] : [];
}

function waveResults() {
  const search = $("wave-search").value.trim().toLocaleLowerCase();
  const inPatch = $("wave-scope").value === "patch";
  const ids = inPatch ? patchWaveIds() : [...waveLibrary.keys()].sort((a, b) =>
    waveLabel(a).localeCompare(waveLabel(b), undefined, { numeric: true }));
  return ids.filter((id) => waveLabel(id).toLocaleLowerCase().includes(search));
}

function renderWaveBrowser() {
  const ids = waveResults();
  const patchIds = patchWaveIds();
  const editable = samplesForPatch && Boolean(activeDraft());
  const list = $("wave-list");
  const adding = editable && $("wave-scope").value === "library";
  $("wave-add").hidden = !adding;
  $("wave-add").disabled = patchIds.length >= maxWaves() || ids.every((id) => patchIds.includes(id));
  $("wave-count").textContent = tr("{0} dans la bibliothèque{1}", waveLibrary.size, editable ? tr(" · {0}/{1} dans ce patch", patchIds.length, maxWaves()) : "");
  const scroll = list.scrollTop;
  list.replaceChildren();
  if (!ids.length) {
    list.append(element("p", tr("Aucune forme d’onde dans cette liste.")));
  }
  for (const id of ids) {
    const row = element("div", "", "wave-row");
    const at = patchIds.indexOf(id);
    const label = waveLabel(id);
    if (editable) {
      const checkbox = element("input", "");
      checkbox.type = "checkbox";
      checkbox.checked = at >= 0;
      checkbox.disabled = at < 0 && patchIds.length >= maxWaves();
      checkbox.setAttribute("aria-label", tr("{0} dans le patch", label));
      checkbox.onchange = () => {
        if (checkbox.checked) setBank([...patchIds, id]);
        else setBank(patchIds.filter((key) => key !== id));
      };
      row.append(checkbox);
    } else {
      row.append(element("span", at >= 0 ? String(at + 1) : "", "wave-slot"));
    }
    const button = element("button", label);
    button.setAttribute("aria-pressed", String(id === previewId));
    button.onclick = () => {
      previewId = id;
      for (const other of list.querySelectorAll("button[aria-pressed=true]")) {
        other.setAttribute("aria-pressed", "false");
      }
      button.setAttribute("aria-pressed", "true");
      renderWaveDetail();
      if (!waveLibrary.has(id) && port && catalog.maxWaves && !busy) readRemoteWave(id);
    };
    row.append(button);
    if ($("wave-scope").value === "library") {
      const remove = element("button", "×", "wave-remove");
      remove.setAttribute("aria-label", tr("Supprimer {0} de la bibliothèque", label));
      remove.title = tr("Supprimer de la bibliothèque");
      remove.onclick = async () => {
        const referenced = [...drafts, ...savedPatches, ...entries].some(({ patch }) => patch.waves?.includes(id));
        if (referenced && !confirm(tr("Ce WAV est référencé par un patch. Supprimer le fichier de la bibliothèque locale ?"))) return;
        remove.disabled = true;
        try {
          await deleteWave(id);
          waveLibrary.delete(id);
          if (previewId === id) previewId = undefined;
          $("wave-message").textContent = tr("{0} supprimé de la bibliothèque.", label);
          renderWaveBrowser();
        } catch {
          $("wave-message").textContent = tr("Impossible de supprimer {0}.", label);
          remove.disabled = false;
        }
      };
      row.append(remove);
    }
    list.append(row);
  }
  list.scrollTop = scroll;
  renderWaveDetail();
}

function renderWaveDetail() {
  const wave = waveLibrary.get(previewId);
  $("wave-name").textContent = previewId ? waveLabel(previewId) : tr("Aperçu");
  $("wave-caption").textContent = wave ? tr("1 024 échantillons · mono") :
    previewId ? tr("Aperçu indisponible localement.") : "";
  $("wave-fetch").hidden = !previewId || Boolean(wave) || !port || !catalog.maxWaves;
  $("wave-fetch").disabled = Boolean(busy);
  drawWave($("wave-preview"), wave);
}

function setBank(ids) {
  const patch = activeDraft()?.patch;
  if (!patch || ids.length > maxWaves()) return;
  const previous = patch.waves ?? [];
  for (const node of patch.modules) {
    if (node.type !== "Osc" || (node.parameters?.Wave ?? 0) < builtinWaves.length) continue;
    const id = previous[node.parameters.Wave - builtinWaves.length];
    const at = ids.indexOf(id);
    if (at < 0) delete node.parameters.Wave;
    else node.parameters.Wave = builtinWaves.length + at;
    if (!Object.keys(node.parameters).length) delete node.parameters;
  }
  if (ids.length) patch.waves = ids;
  else delete patch.waves;
  changed(true);
  renderWaveBrowser();
}

function element(tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  return node;
}

function current() {
  if (!selection) return undefined;
  const list = selection.kind === "draft" ? drafts : selection.kind === "usb" ? entries : examples;
  return list.find((item) => item.key === selection.key);
}

function activeDraft() {
  return selection?.kind === "draft" ? current() : undefined;
}

function showError(message) {
  $("error").textContent = message || storageError;
  $("error").hidden = !$("error").textContent;
}

try {
  drafts = JSON.parse(localStorage.getItem(draftStorage) ?? "[]").map((draft) => ({
    ...draft, patch: normalizePatch(draft.patch),
  }));
  savedPatches = JSON.parse(localStorage.getItem("modulox.saved.v1") ?? "[]");
  savedPatches.push(...drafts.slice(1));
  drafts = drafts.slice(0, 1);
  localStorage.setItem("modulox.saved.v1", JSON.stringify(savedPatches));
  localStorage.setItem(draftStorage, JSON.stringify(drafts));
  if (drafts.length) selection = { kind: "draft", key: drafts[0].key };
} catch {
  storageError = tr("Les patches locaux n’ont pas pu être relus.");
}

function persist() {
  try {
    localStorage.setItem(draftStorage, JSON.stringify(drafts));
    storageError = "";
  } catch {
    storageError = tr("Impossible de sauvegarder les brouillons dans ce navigateur.");
  }
  showError("");
}

function select(kind, key) {
  selection = { kind, key };
  selectedNode = cableSource = undefined;
  cableMidiPort = cableEventPort = undefined;
  viewport.scrollTo(0, 0);
  render();
}

function showScreen(next) {
  screen = next;
  $("patch-home").hidden = screen !== "patches";
  $("patch-editor").hidden = screen !== "editor";
  $("wave-screen").hidden = screen !== "waveforms";
  $("sample-screen").hidden = screen !== "samples";
  if (screen !== "samples") samplePlayback?.stop();
  for (const name of ["patches", "editor", "waveforms", "samples"]) {
    const button = "nav-" + name;
    if (screen === name) $(button).setAttribute("aria-current", "page");
    else $(button).removeAttribute("aria-current");
  }
  render();
}

function switchEditorView(view) {
  editorView = view;
  $("graph-view").hidden = view !== "graph";
  $("parameter-view").hidden = view !== "parameters";
  $("tab-graph").setAttribute("aria-pressed", String(view === "graph"));
  $("tab-parameters").setAttribute("aria-pressed", String(view === "parameters"));
  if (screen === "editor") render();
}

function createDraft(source) {
  if (busy) return;
  if (drafts.length && !confirm(tr("Remplacer le patch en cours ? Enregistre-le dans ton dossier si tu veux le conserver."))) return;
  const patch = source ? structuredClone(source.patch) : {
    version: 3, name: tr("Nouveau patch"), modules: [], connections: [],
  };
  patch.layout = source ? { ...layout(patch), ...patch.layout } : { out: { x: 232, y: 285 } };
  const file = source?.file;
  let key = source?.key?.replace(/\.json$/, "");
  if (file && !file.startsWith("u")) {
    let hash = 2166136261;
    for (const byte of new TextEncoder().encode(file)) hash = Math.imul(hash ^ byte, 16777619) >>> 0;
    key = "f" + hash.toString(16).padStart(8, "0");
  }
  if (!file) key = "u" + crypto.randomUUID().replaceAll("-", "").slice(0, 13);
  drafts = [{ key, patch, targetFile: selection?.kind === "usb" ? file : undefined }];
  if (selection?.kind !== "usb" && key.startsWith("f")) drafts[0].key = "u" + crypto.randomUUID().replaceAll("-", "").slice(0, 13);
  persist();
  switchEditorView("graph");
  select("draft", drafts[0].key);
  showScreen("editor");
  return true;
}

function serialized(patch) {
  return JSON.stringify(patch);
}

function draftStatus(draft) {
  const remote = entries.find((item) => item.file === (draft.targetFile ?? draft.key + ".json"));
  const snapshot = serialized(draft.patch);
  if (remote && snapshot === serialized(remote.patch)) return tr("Sur le Modulox");
  if (draft.lastSent) {
    if (port && hasRead && !remote) return tr("À renvoyer");
    if (remote && snapshot === draft.lastSent) return tr("Copie USB différente");
    return snapshot === draft.lastSent ? tr("Dernier envoi effectué") : tr("Modifications locales");
  }
  return draft.targetFile ? tr("Modification du patch Modulox") : tr("En cours de création");
}

function renderLibrary() {
  function rows(container, items, kind, status) {
    container.replaceChildren();
    for (const item of items) {
      const button = element("button", item.patch.name || tr("Sans nom"), "patch");
      button.append(element("small", status(item)));
      button.setAttribute("aria-current", String(selection?.kind === kind && selection.key === item.key));
      button.onclick = () => {
        $("home-message").textContent = "";
        select(kind, item.key);
      };
      container.append(button);
    }
  }
  rows($("drafts"), drafts, "draft", draftStatus);
  rows($("patches"), hasRead ? entries : examples, hasRead ? "usb" : "example",
    (item) => item.patch.modules.length + " modules");
  $("bank-title").textContent = hasRead ? tr("Patches du Modulox") : tr("Patches de démonstration");
  $("source").textContent = hasRead && !port ? tr("Dernière lecture") : String((hasRead ? entries : examples).length);
  $("draft-section").hidden = !drafts.length;
  if (hasRead && !entries.length) $("patches").append(element("p", tr("Aucun patch sur le Modulox."), "empty-state"));
}

function renderActions() {
  midiKeyboard.setBusy(Boolean(busy));
  $("language-picker").inert = Boolean(busy);
  const item = current();
  const draft = activeDraft();
  renderFirmware();
  viewport.inert = Boolean(busy);
  $("palette").inert = Boolean(busy);
  $("details").inert = Boolean(busy);
  $("name").disabled = Boolean(busy);
  $("patches").inert = $("drafts").inert = Boolean(busy);
  $("status").textContent = busy || (port ? (canUpload ? tr("Modulox connecté") : tr("Firmware en lecture seule")) : tr("USB déconnecté"));
  $("status").toggleAttribute("data-connected", Boolean(port) && !busy);
  $("connect").textContent = port ? tr("Déconnecter") : tr("Connecter en USB");
  $("connect").disabled = Boolean(busy) || !navigator.serial;
  $("refresh").disabled = Boolean(busy) || !port;
  $("nav-editor").disabled = !drafts.length || Boolean(busy);
  $("home-name").textContent = item?.patch.name || tr("Aucun patch sélectionné");
  $("home-source").textContent = item ? (draft ? tr("En cours d’édition") : selection.kind === "usb" ? tr("Sur le Modulox") : tr("Exemple")) : "";
  $("home-summary").textContent = item
    ? [tr("{0} modules", item.patch.modules.length), tr("{0} formes d’onde importées", item.patch.waves?.length ?? 0)].join(" · ")
    : tr("Connecte le Modulox ou crée un patch pour commencer.");
  $("save-home").hidden = !item;
  $("save-home").disabled = Boolean(busy);
  $("export-home").hidden = !item;
  $("export-home").disabled = !item || Boolean(busy);
  $("export-patch").disabled = !item || Boolean(busy);
  if (draft && $("name").value !== draft.patch.name) $("name").value = draft.patch.name;
  $("summary").textContent = item ? [
    draft ? draftStatus(draft) : item.file,
    tr("{0} modules", item.patch.modules.length),
    tr("{0} câbles", item.patch.connections.length),
    ...(item.patch.modulations?.length ? [tr("{0} modulations", item.patch.modulations.length)] : []),
  ].join(" · ") : "";
  $("settings").hidden = !item || !catalog.settings;
  $("settings").setAttribute("aria-pressed", String(selectedNode === "$settings"));
  $("copy").hidden = !item;
  $("copy").textContent = draft ? tr("Reprendre l’édition") : tr("Configurer");
  $("upload").hidden = !draft;
  const samplerError = draft ? patchSampleError(draft.patch) : "";
  const timingError = draft && port && !deviceCatalog?.clockRouting &&
    (draft.patch.modules.some((node) => timingModuleTypes.includes(node.type)) || draft.patch.routings?.length)
    ? tr("Ces modules demandent le nouveau firmware d’horloge.") : "";
  const vaError = draft && port && !deviceCatalog?.vaExpansion && usesVaExpansion(draft.patch)
    ? tr("Ces fonctions demandent le nouveau firmware VA : unisson, glide et nouveaux modules.") : "";
  const stereoError = draft && port && !deviceCatalog?.stereoGraph
    ? tr("Ce graphe demande le nouveau firmware stéréo.") : "";
  const fmError = draft && port && !deviceCatalog?.fm4 && usesFm4(draft.patch)
    ? tr("FM4 et le couplage des Osc demandent le nouveau firmware FM.") : "";
  $("upload").disabled = !port || !canUpload || Boolean(busy) || Boolean(samplerError || timingError || vaError || stereoError || fmError) ||
    Boolean(draft?.key.startsWith("f") && !catalog.patchManagement);
  $("upload").textContent = draft?.targetFile ? tr("Remplacer sur le Modulox") : tr("Envoyer au Modulox");
  $("delete-patch").hidden = selection?.kind !== "usb";
  $("delete-patch").disabled = !port || !catalog.patchManagement || Boolean(busy);
  $("delete-patch").title = catalog.patchManagement ? "" : tr("La suppression nécessite le nouveau firmware.");
  $("save-local").disabled = !item || Boolean(busy);
  $("new-patch").disabled = $("load-local").disabled = Boolean(busy);
  $("load-factory").disabled = $("import-patch").disabled = Boolean(busy);
  $("choose-folder").disabled = $("refresh-folder").disabled = Boolean(busy);
  $("personal-bank").inert = $("server-patch-list").inert = Boolean(busy);
  $("factory-upload-all").disabled = Boolean(busy) || !port || !canUpload || Boolean(currentFirmware?.pendingVerify);
  $("factory-upload-stop").hidden = !factoryAbort;
  $("copy").disabled = Boolean(busy);
  $("discard-draft").hidden = !draft;
  $("discard-draft").disabled = Boolean(busy);
  $("modules-toggle").disabled = !draft;
  $("parameter-view").inert = Boolean(busy);
  if (screen === "samples") renderSampleDetail();
  $("upload").title = samplerError || timingError || vaError || stereoError || fmError || (draft?.key.startsWith("f") && !catalog.patchManagement
    ? tr("Le remplacement d’un patch d’usine nécessite le nouveau firmware.")
    : canUpload ? "" : tr("Connecte un Modulox avec le firmware compatible."));
  $("arrange").hidden = !draft;
  $("hint").textContent = cableSource ? (cableEventPort ? tr("Choisis une entrée Trigger, Reset ou Run compatible.")
    : cableMidiPort
    ? tr("Choisis l’entrée MIDI violette de même nom. Échap annule le câble.")
    : isModulationSource(cableSource)
    ? tr("Choisis une entrée de modulation bleue. Échap annule le câble.")
    : tr("Choisis une entrée audio. Échap annule le câble."))
    : samplerError || timingError || vaError || stereoError || fmError || (draft ? (editorView === "graph" ? tr("Dépose des modules mono ou stéréo. Pan ou Stereo convertissent une source mono.") : "")
      : item ? tr("Sélectionne un module pour voir ses paramètres.") : "");
}

function samplerUnavailable() {
  if (!port || !hasRead || deviceCatalog?.maxSampleBytes) return "";
  const reasons = {
    "partition-missing": tr("Sampler indisponible sur le Modulox : partition de samples absente."),
    "storage-not-empty": tr("Sampler indisponible sur le Modulox : la partition contient des données non reconnues."),
    "storage-error": tr("Sampler indisponible sur le Modulox : erreur de stockage."),
    "psram-unavailable": tr("Sampler indisponible sur le Modulox : PSRAM inaccessible."),
  };
  return reasons[deviceCatalog?.sampleStatus] ?? tr("Sampler indisponible sur le Modulox connecté.");
}

function patchSampleError(patch) {
  const types = patch.modules.map((node) => node.type).filter((type) => sampleModuleTypes.includes(type));
  if (!types.length || !port) return "";
  return samplerUnavailable() || (types.some((type) => !deviceCatalog?.modules.some((module) => module.type === type))
    ? tr("Le Granulizer nécessite le nouveau firmware.") : "");
}

function renderPalette() {
  const patch = activeDraft()?.patch;
  const palette = $("palette");
  palette.replaceChildren();
  for (const spec of catalog.modules) {
    const count = patch?.modules.filter((node) => node.type === spec.type).length ?? 0;
    const button = element("button", tr(spec.type), "palette-module");
    button.append(element("small", count + "/" + spec.limit));
    button.disabled = !patch || count >= spec.limit;
    if (spec.type === "Sampler") button.title = samplerUnavailable();
    button.draggable = !button.disabled;
    button.onclick = () => addModule(spec.type);
    button.ondragstart = (event) => {
      event.dataTransfer.setData("application/x-modulox-module", spec.type);
      event.dataTransfer.effectAllowed = "copy";
    };
    palette.append(button);
  }
}

function layout(patch) {
  const nodes = [...patch.modules, outputModule];
  const pending = new Set(nodes.map((node) => node.id));
  const levels = [];
  const depth = new Map();
  while (pending.size) {
    const ready = [...pending].filter((id) =>
      patch.connections.every(([from, to]) => to !== id || !pending.has(from)));
    if (!ready.length) ready.push(...pending);
    for (const id of ready) {
      pending.delete(id);
      const inputs = patch.connections.filter(([, to]) => to === id);
      const level = Math.max(0, ...inputs.map(([from]) => (depth.get(from) ?? 0) + 1));
      depth.set(id, level);
      (levels[level] ??= []).push(id);
    }
  }
  const width = Math.max(viewport.clientWidth, 430, 175 * Math.max(...levels.map((row) => row.length)) + margin);
  const arranged = Object.create(null);
  let y = margin;
  for (const row of levels) {
    row.forEach((id, i) => {
      arranged[id] = {
        x: Math.round((width - nodeWidth) / 2 + (i - (row.length - 1) / 2) * 175),
        y,
      };
    });
    y += Math.max(52, ...row.map((id) => nodeHeight(nodes.find((node) => node.id === id)?.type))) + 46;
  }
  return arranged;
}

function isModulationSource(id) {
  const node = current()?.patch.modules.find((module) => module.id === id);
  return Boolean(moduleSpec(catalog, node?.type)?.controlOutput);
}

function portPosition(index, count) {
  return (index + 1) / (count + 1);
}

function topPortPosition(spec, index, audio = false) {
  const inputs = spec?.inputs ?? 0;
  const controls = (spec?.midiInputs?.length ?? 0) + (spec?.triggerInputs?.length ?? 0);
  return portPosition(index + (audio ? 0 : inputs), inputs + controls);
}

function drawWires(patch, preview) {
  const scale = window.devicePixelRatio || 1;
  const width = Math.max(viewport.clientWidth, 430, ...[...positions.values()].map(({ x }) => x + nodeWidth + margin));
  const height = Math.max(viewport.clientHeight, 1800, ...[...positions.values()].map(({ y, height }) => y + height + 300));
  graph.style.width = canvas.style.width = width + "px";
  graph.style.height = canvas.style.height = height + "px";
  if (canvas.width !== Math.round(width * scale)) canvas.width = Math.round(width * scale);
  if (canvas.height !== Math.round(height * scale)) canvas.height = Math.round(height * scale);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  ctx.clearRect(0, 0, width, height);
  ctx.lineWidth = 2;
  ctx.strokeStyle = "#e3a86d";
  function wire(from, x, y, modulation = false, midiPort, eventPort) {
    const source = positions.get(from);
    if (!source) return;
    const type = patch.modules.find((node) => node.id === from)?.type;
    const output = eventPort ?? midiPort;
    const outputs = (eventPort ? moduleSpec(catalog, type)?.triggerOutputs : moduleSpec(catalog, type)?.midiOutputs) ?? [];
    const horizontal = modulation && !output;
    const sx = source.x + nodeWidth * (output
      ? portPosition(outputs.indexOf(output), outputs.length) : horizontal ? 1 : 0.5);
    const sy = source.y + (horizontal ? source.height / 2 : source.height);
    const bend = Math.max(25, Math.abs(y - sy) * 0.45);
    ctx.beginPath();
    ctx.moveTo(sx, sy);
    if (horizontal) {
      const offset = Math.max(30, Math.abs(x - sx) * 0.45);
      ctx.bezierCurveTo(sx + offset, sy, x - offset, y, x, y);
    } else {
      ctx.bezierCurveTo(sx, sy + bend, x, y - bend, x, y);
    }
    ctx.stroke();
  }
  for (const [from, to, input] of patch.connections) {
    const target = positions.get(to);
    const node = to === "out" ? outputModule : patch.modules.find((node) => node.id === to);
    const inputs = node && moduleSpec(catalog, node.type)?.inputs;
    if (target && inputs && input >= 0 && input < inputs) {
      const sourceType = patch.modules.find(n => n.id === from)?.type;
      const stereo = moduleSpec(catalog, sourceType)?.outputChannels === 2;
      ctx.strokeStyle = stereo ? "#72d6cb" : "#e3a86d";
      ctx.lineWidth = stereo ? 3 : 2;
      wire(from, target.x + nodeWidth * topPortPosition(moduleSpec(catalog, node.type), input, true), target.y);
    }
  }
  for (const [from, to, name, midiPort] of patch.modulations ?? []) {
    ctx.lineWidth = 2;
    ctx.strokeStyle = midiPort ? "#bd95f2" : "#81aefa";
    const target = positions.get(to);
    const node = to === "out" ? outputModule : patch.modules.find((item) => item.id === to);
    const spec = nodeSpec(patch, catalog, node);
    const ports = midiPort ? [...(spec?.midiInputs ?? []), ...(spec?.triggerInputs ?? [])]
      : [...(spec?.modulationInputs ?? []), ...(spec?.cvInputs ?? [])];
    const index = ports.indexOf(name);
    if (target && index >= 0) {
      const offset = midiPort ? topPortPosition(spec, index) : portPosition(index, ports.length);
      wire(from, target.x + (midiPort ? nodeWidth * offset : 0),
        target.y + (midiPort ? 0 : target.height * offset), true, midiPort);
    }
  }
  for (const [from, to, input, output] of patch.routings ?? []) {
    ctx.lineWidth = 2;
    const target = positions.get(to);
    const spec = moduleSpec(catalog, patch.modules.find((node) => node.id === to)?.type);
    if (!target || !spec) continue;
    const cv = input === "Value";
    const ports = cv ? [...(spec.modulationInputs ?? []), ...(spec.cvInputs ?? [])]
      : [...(spec.midiInputs ?? []), ...(spec.triggerInputs ?? [])];
    const at = ports.indexOf(input);
    if (at < 0) continue;
    ctx.strokeStyle = cv ? "#81aefa" : "#80d6b2";
    const sourceSpec = moduleSpec(catalog, patch.modules.find((node) => node.id === from)?.type);
    const eventOutput = sourceSpec?.triggerOutputs?.length ? output : undefined;
    wire(from, target.x + (cv ? 0 : nodeWidth * topPortPosition(spec, at)),
      target.y + (cv ? target.height * portPosition(at, ports.length) : 0),
      true, cv || eventOutput ? undefined : output, eventOutput);
  }
  if (cableSource && preview) {
    const modulation = isModulationSource(cableSource);
    const stereo = moduleSpec(catalog, patch.modules.find(n => n.id === cableSource)?.type)?.outputChannels === 2;
    ctx.lineWidth = !modulation && stereo ? 3 : 2;
    ctx.strokeStyle = cableEventPort ? "#80d6b2" : cableMidiPort ? "#bd95f2" : modulation ? "#81aefa" : stereo ? "#72d6cb" : "#e3a86d";
    ctx.setLineDash([4, 4]);
    wire(cableSource, preview.x, preview.y, modulation, cableMidiPort, cableEventPort);
    ctx.setLineDash([]);
  }
}

function point(event) {
  const bounds = graph.getBoundingClientRect();
  return { x: event.clientX - bounds.left, y: event.clientY - bounds.top };
}

function setCable(id, midiPort, eventPort) {
  cableSource = id;
  cableMidiPort = midiPort;
  cableEventPort = eventPort;
  markCableTargets();
  renderActions();
  if (current()) drawWires(current().patch);
}

function cableError(patch, to, input, modulation, midiPort, routing = false) {
  if (routing) return routingError(patch, catalog, cableSource, to, input, cableEventPort ?? cableMidiPort ?? "CV");
  if (cableEventPort) return tr("Relie cette sortie à une entrée de déclenchement.");
  if (modulation !== isModulationSource(cableSource) || midiPort !== Boolean(cableMidiPort)) {
    return tr("Ces ports sont de types différents.");
  }
  return modulation ? modulationError(patch, catalog, cableSource, to, input, cableMidiPort)
    : connectionError(patch, catalog, cableSource, to, input);
}

function markCableTargets() {
  const patch = activeDraft()?.patch;
  graph.classList.toggle("cabling", Boolean(cableSource));
  graph.querySelectorAll("[data-from]").forEach((port) =>
    port.setAttribute("aria-pressed", String(port.dataset.from === cableSource && port.dataset.midiPort === cableMidiPort &&
      port.dataset.eventPort === cableEventPort)));
  graph.querySelectorAll("[data-to]").forEach((port) => {
    const { to, modulation, input, midiInput, routing } = port.dataset;
    port.toggleAttribute("data-compatible", Boolean(patch && cableSource &&
      !cableError(patch, to, routing ?? modulation ?? Number(input), Boolean(modulation), Boolean(midiInput), Boolean(routing))));
  });
}

function connectCable(to, input, modulation = false, midiPort = false, routing = false) {
  const patch = activeDraft()?.patch;
  if (!patch || !cableSource) return;
  const error = cableError(patch, to, input, modulation, midiPort, routing);
  if (error) return showError(error);
  const key = routing ? "routings" : modulation ? "modulations" : "connections";
  patch[key] = (patch[key] ?? []).filter(([, target, port, midiOutput]) =>
    routing ? target !== to || port !== input : target !== to || Boolean(midiOutput) !== midiPort || (port !== input &&
      !(midiPort && ["Gate", "Trigger"].includes(port) && ["Gate", "Trigger"].includes(input))));
  const edge = [cableSource, to, input];
  if (routing) edge.push(cableEventPort ?? cableMidiPort ?? "CV");
  else if (midiPort) edge.push(cableMidiPort);
  patch[key].push(edge);
  cableSource = undefined;
  cableMidiPort = undefined;
  cableEventPort = undefined;
  selectedNode = to;
  changed();
}

function markSelection() {
  graph.querySelectorAll("[data-node]").forEach((node) =>
    node.toggleAttribute("data-selected", node.dataset.node === selectedNode));
}

function selectNode(id) {
  selectedNode = id;
  markSelection();
  renderActions();
  renderInspector();
}

function startNodeDrag(event, node, card) {
  if (!activeDraft() || event.button !== 0) return;
  const origin = point(event);
  const start = positions.get(node.id);
  drag = { kind: "node", id: node.id, card, origin, start, pointer: event.pointerId, moved: false };
}

function renderGraph() {
  graph.replaceChildren(canvas);
  positions = new Map();
  const patch = current()?.patch;
  if (!patch) {
    graph.removeAttribute("style");
    canvas.width = canvas.height = 0;
    return;
  }
  const arranged = layout(patch);
  const editable = Boolean(activeDraft());
  for (const node of [...patch.modules, outputModule]) {
    const coords = patch.layout?.[node.id];
    const position = Number.isFinite(coords?.x) && Number.isFinite(coords?.y) ? coords : arranged[node.id];
    const height = nodeHeight(node.type);
    positions.set(node.id, { ...position, height });
    const spec = nodeSpec(patch, catalog, node);
    const card = element("div", "", node.id === "out" ? "node output" : spec?.controlOutput ? "node control" : "node");
    card.dataset.node = node.id;
    card.style.left = position.x + "px";
    card.style.top = position.y + "px";
    card.style.height = height + "px";
    const label = element("button", "", "node-label");
    label.append(element("strong", tr(node.type)), element("small", node.id === "out" ? tr("Audio stéréo") :
      node.id + (spec?.controlOutput ? "" : spec?.outputChannels === 2 ? " · S" : " · M")));
    label.onclick = () => selectNode(node.id);
    label.onpointerdown = (event) => startNodeDrag(event, node, card);
    card.append(label);
    for (let i = 0; i < (spec?.inputs ?? 0); i++) {
      const inlet = element(editable ? "button" : "span", "", "port input");
      if (spec.inputChannels === 2) inlet.classList.add("stereo-port");
      inlet.style.left = topPortPosition(spec, i, true) * 100 + "%";
      const inputLabel = spec.inputLabels?.[i] ?? (spec.inputs > 1 ? tr("Entrée {0}", i + 1) : tr("Entrée audio"));
      inlet.title = inputLabel + (spec.inputChannels === 2 ? tr(" · stéréo L/R") : " · mono");
      if (editable) {
        inlet.dataset.to = node.id;
        inlet.dataset.input = i;
        inlet.setAttribute("aria-label", node.id + " · " + inlet.title);
        inlet.onclick = () => cableSource ? connectCable(node.id, i) : selectNode(node.id);
      }
      card.append(inlet);
    }
    for (const [names, midi] of [
      [[...(spec?.modulationInputs ?? []), ...(spec?.cvInputs ?? [])], false],
      [[...(spec?.midiInputs ?? []), ...(spec?.triggerInputs ?? [])], true]]) {
      for (const [index, name] of names.entries()) {
        const routing = (midi ? spec?.triggerInputs : spec?.cvInputs)?.includes(name);
        const inlet = element(editable ? "button" : "span", "", midi
          ? routing ? "port input event-port" : "port input midi-port" : "port mod-input");
        inlet.style[midi ? "left" : "top"] = (midi ? topPortPosition(spec, index) : portPosition(index, names.length)) * 100 + "%";
        inlet.title = tr(routing ? midi ? "Horloge" : "CV" : midi ? "MIDI" : "Modulation") + " · " + tr(name);
        if (editable) {
          inlet.dataset.to = node.id;
          if (routing) inlet.dataset.routing = name;
          else {
            inlet.dataset.modulation = name;
            if (midi) inlet.dataset.midiInput = name;
          }
          inlet.setAttribute("aria-label", node.id + " · " + inlet.title);
          inlet.onclick = () => cableSource ? connectCable(node.id, name, true, midi && !routing, routing) : selectNode(node.id);
        }
        card.append(inlet);
      }
    }
    const eventOutputs = spec?.triggerOutputs?.length ? spec.triggerOutputs : undefined;
    for (const [index, output] of (node.id === "out" ? [] : eventOutputs ?? spec?.midiOutputs ?? [undefined]).entries()) {
      const midiPort = eventOutputs ? undefined : output;
      const eventPort = eventOutputs ? output : undefined;
      const outlet = element(editable ? "button" : "span", "",
        eventPort ? "port output-port event-port" : midiPort ? "port output-port midi-port" :
        spec?.controlOutput ? "port mod-output" : "port output-port");
      outlet.title = eventPort ? "Horloge · " + eventPort : midiPort ? "MIDI · " + midiPort : spec?.controlOutput ? tr("Sortie de modulation") : tr("Sortie audio");
      if (!spec?.controlOutput && spec?.outputChannels === 2) {
        outlet.classList.add("stereo-port");
        outlet.title = tr("Sortie stéréo L/R");
      }
      if (output) {
        outlet.style.left = portPosition(index, (eventOutputs ?? spec.midiOutputs).length) * 100 + "%";
      }
      if (editable) {
        outlet.dataset.from = node.id;
        if (midiPort) outlet.dataset.midiPort = midiPort;
        if (eventPort) outlet.dataset.eventPort = eventPort;
        outlet.setAttribute("aria-label", node.id + " · " + outlet.title);
        const active = () => cableSource === node.id && cableMidiPort === midiPort && cableEventPort === eventPort;
        outlet.setAttribute("aria-pressed", String(active()));
        outlet.onclick = () => active() ? setCable(undefined) : setCable(node.id, midiPort, eventPort);
        outlet.onpointerdown = (event) => {
          if (event.button !== 0) return;
          drag = { kind: "cable", id: node.id, midiPort, eventPort, origin: point(event),
            pointer: event.pointerId, moved: false };
        };
      }
      card.append(outlet);
    }
    graph.append(card);
  }
  markSelection();
  markCableTargets();
  drawWires(patch);
}

window.addEventListener("pointermove", (event) => {
  if (!drag || drag.pointer !== event.pointerId) return;
  const cursor = point(event);
  const dx = cursor.x - drag.origin.x;
  const dy = cursor.y - drag.origin.y;
  if (!drag.moved && Math.hypot(dx, dy) < 4) return;
  const patch = activeDraft()?.patch;
  if (!patch) return;
  if (drag.kind === "cable") {
    if (!drag.moved) setCable(drag.id, drag.midiPort, drag.eventPort);
    drawWires(patch, cursor);
  } else {
    const position = {
      x: Math.max(12, Math.min(graph.clientWidth - nodeWidth - 12, Math.round(drag.start.x + dx))),
      y: Math.max(12, Math.round(drag.start.y + dy)),
      height: drag.start.height,
    };
    drag.card.style.left = position.x + "px";
    drag.card.style.top = position.y + "px";
    positions.set(drag.id, position);
    drawWires(patch);
  }
  drag.moved = true;
});

window.addEventListener("pointerup", (event) => {
  if (!drag || drag.pointer !== event.pointerId) return;
  const finished = drag;
  drag = undefined;
  if (!finished.moved || !activeDraft()) return;
  if (finished.kind === "cable") {
    const inlet = document.elementFromPoint(event.clientX, event.clientY)?.closest("[data-to]");
    if (inlet) {
      const modulation = inlet.dataset.modulation;
      const routing = inlet.dataset.routing;
      connectCable(inlet.dataset.to, routing ?? modulation ?? Number(inlet.dataset.input), Boolean(modulation),
        Boolean(inlet.dataset.midiInput), Boolean(routing));
    }
    if (cableSource && current()) drawWires(current().patch);
  } else {
    const patch = activeDraft().patch;
    const { x, y } = positions.get(finished.id);
    (patch.layout ??= {})[finished.id] = { x, y };
    selectedNode = finished.id;
    changed();
  }
});

window.addEventListener("pointercancel", () => {
  drag = undefined;
  cableSource = undefined;
  cableMidiPort = cableEventPort = undefined;
  render();
});
window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && cableSource) setCable(undefined);
});
viewport.addEventListener("dragover", (event) => {
  if (activeDraft() && event.dataTransfer.types.includes("application/x-modulox-module")) {
    event.preventDefault();
    event.dataTransfer.dropEffect = "copy";
  }
});
viewport.addEventListener("drop", (event) => {
  const type = event.dataTransfer.getData("application/x-modulox-module");
  if (!type || !activeDraft()) return;
  event.preventDefault();
  const cursor = point(event);
  addModule(type, { x: Math.max(12, Math.round(cursor.x - nodeWidth / 2)),
    y: Math.max(12, Math.round(cursor.y - nodeHeight(type) / 2)) });
});

function addModule(type, position) {
  const patch = activeDraft()?.patch;
  const spec = moduleSpec(catalog, type);
  if (!patch || !spec?.limit || patch.modules.filter((node) => node.type === type).length >= spec.limit) return;
  const used = new Set(patch.modules.map((node) => node.id));
  const occupied = [...positions.values()];
  const base = type.toLowerCase().replace(/[^a-z0-9]/g, "");
  let id = base;
  for (let i = 2; used.has(id); i++) id = base + i;
  patch.modules.push({ id, type });
  if (!position) {
    let index = 0;
    do {
      position = { x: margin + viewport.scrollLeft + (index % 2) * 158,
        y: margin + viewport.scrollTop + Math.floor(index / 2) * 82 };
      index++;
    } while (occupied.some(({ x, y, height }) =>
      Math.abs(x - position.x) < nodeWidth + 12 &&
      position.y < y + height + 12 && y < position.y + nodeHeight(type) + 12));
  }
  (patch.layout ??= {})[id] = position;
  selectedNode = id;
  changed();
}

function parameterControl(owner, parameter, patch, key = "parameters", onValue, slider = false) {
  const value = owner[key]?.[parameter.name] ?? parameter.initial;
  const hasSlider = slider && !parameter.choices;
  const label = element(hasSlider ? "div" : "label", "", "parameter");
  const labelText = tr(owner.type === "Delay" && parameter.name === "Time" ? "Time L" :
    owner.type === "FM4" ? parameter.name.replace(/^[1-4] /, "") : parameter.name);
  const externalTrigger = owner.type === "Arpeggiator" && parameter.name === "Division" &&
    patch.routings?.some(([, to, input]) => to === owner.id && input === "Trigger");
  const noFmSource = owner.type === "Osc" && parameter.name === "FM" && patch.modules.filter(n=>n.type==="Osc").length < 2;
  const disabledReason = externalTrigger ? tr("Pilotée par le câble Trigger. Débranche-le pour utiliser cette division.") :
    noFmSource ? tr("Ajoute un deuxième Osc : il modulera le premier, même avec son Level à zéro.") : "";
  if (disabledReason) {
    label.classList.add("parameter-inactive");
    label.title = disabledReason;
  }
  label.append(element("span", labelText));
  const target = key === "settings" ? "$settings" : owner.id;
  const binding = patch.midiLearn?.find(([id, name]) => id === target && name === parameter.name);
  if (binding) {
    label.firstChild.append(element("small", " CC " + binding[2], "midi-learn-label"));
  }
  const editor = element("span", "", "value");
  const control = element(parameter.choices ? "select" : "input", "");
  if (parameter.choices) {
    parameter.choices.forEach((choice, index) => {
      const option = element("option", tr(choice));
      option.value = index + parameter.min;
      control.append(option);
    });
  } else {
    control.type = "number";
    control.min = parameter.min;
    control.max = parameter.max;
    control.step = owner.type === "Osc" && parameter.name === "Pitch" ? parameter.step ?? 1 : 1;
  }
  control.value = value;
  control.title = tr("Valeur par défaut : {0}", tr(parameter.choices?.[parameter.initial - parameter.min] ?? parameter.initial));
  control.disabled = !activeDraft() || Boolean(disabledReason);
  if (disabledReason) control.title = disabledReason;
  let range;
  function saveValue(next) {
    if (range) range.value = next;
    if (next === parameter.initial) delete owner[key]?.[parameter.name];
    else (owner[key] ??= {})[parameter.name] = next;
    if (owner[key] && !Object.keys(owner[key]).length) delete owner[key];
    if (activeDraft()?.patch === patch) changed(false);
    onValue?.(next);
  }
  control.oninput = () => {
    const next = Number(control.value);
    if (control.value !== "" && Number.isInteger(next) && next >= parameter.min && next <= parameter.max) {
      saveValue(next);
    }
  };
  control.onchange = () => {
    const parsed = control.value === "" ? parameter.initial : Number(control.value);
    const next = Number.isFinite(parsed)
      ? Math.max(parameter.min, Math.min(parameter.max, Math.round(parsed))) : parameter.initial;
    control.value = next;
    saveValue(next);
  };
  editor.append(control);
  if (parameter.unit) editor.append(element("small", parameter.unit));
  label.append(editor);
  if (hasSlider) {
    control.setAttribute("aria-label", labelText);
    range = element("input", "", "parameter-range");
    range.type = "range";
    range.min = parameter.min;
    range.max = parameter.max;
    range.step = control.step;
    range.value = value;
    range.disabled = control.disabled;
    range.setAttribute("aria-label", tr("Curseur {0}", labelText));
    range.oninput = () => {
      control.value = range.value;
      saveValue(Number(range.value));
    };
    label.append(range);
  }
  return label;
}

function oscillatorWavePreview(node, patch) {
  const canvas = element("canvas", "", "osc-wave-preview");
  canvas.width = 320;
  canvas.height = 120;
  canvas.setAttribute("role", "img");
  canvas.setAttribute("aria-label", tr("Courbe de la forme d’onde de l’oscillateur"));
  const fetch = element("button", tr("Lire la forme d’onde depuis le Modulox"));
  const update = () => {
    const wave = node.parameters?.Wave ?? 0;
    const id = patch.waves?.[wave - builtinWaves.length];
    const data = waveLibrary.get(id);
    drawWave(canvas, data, wave < builtinWaves.length ? wave : undefined, node.parameters?.PWM ?? 50);
    fetch.hidden = !id || Boolean(data) || !port || !catalog.maxWaves;
    fetch.onclick = () => readRemoteWave(id);
  };
  return { canvas, fetch, update };
}

function patchCategoryControl(patch) {
  const row = element("label", "", "parameter");
  row.append(element("span", tr("Catégorie")));
  const select = element("select");
  for (const [id, label] of patchCategories) {
    const option = element("option", tr(label));
    option.value = id;
    select.append(option);
  }
  select.value = patch.category ?? "user";
  select.disabled = !activeDraft();
  select.title = tr("Classement dans le menu des patches du Modulox (firmware 1.1 ou suivant).");
  select.onchange = () => { patch.category = select.value; changed(false); };
  row.append(select);
  return row;
}

function renderInspector() {
  const details = $("details");
  details.replaceChildren();
  const patch = current()?.patch;
  if (patch && selectedNode === "$settings" && catalog.settings) {
    details.append(element("h3", tr("Réglages du patch")));
    details.append(patchCategoryControl(patch));
    for (const parameter of catalog.settings.parameters) {
      details.append(parameterControl(patch, parameter, patch, "settings"));
    }
    return;
  }
  const node = selectedNode === "out" ? outputModule : patch?.modules.find((node) => node.id === selectedNode);
  if (!node || !patch) {
    details.append(element("p", tr("Sélectionne un module dans le graphe.")));
    return;
  }
  details.append(element("h3", tr(node.type)), element("p", node.id));
  renderNodeControls(details, node, patch);
  for (const key of ["connections", "modulations", "routings"]) {
    const cables = (patch[key] ?? []).filter(([from, to]) => from === node.id || to === node.id);
    if (!cables.length) continue;
    details.append(element("h4", key === "routings" ? tr("Horloge et CV") : key === "modulations" ? tr("Modulations") : tr("Câbles audio")));
    for (const edge of cables) {
      const [from, to, input] = edge;
      const row = element("div", "", "cable");
      const label = key === "connections" ? input + 1 : input;
      row.append(element("span", from + " → " + (to === "out" ? tr("Sortie") : to) + " · " + label));
      if (activeDraft()) {
        const remove = element("button", "×", "remove-cable");
        remove.setAttribute("aria-label", tr("Retirer le câble {0} vers {1}, entrée {2}", from, to, label));
        remove.onclick = () => {
          patch[key] = patch[key].filter((other) => other !== edge);
          if (key !== "connections" && !patch[key].length) delete patch[key];
          changed();
        };
        row.append(remove);
      }
      details.append(row);
    }
  }
  if (activeDraft() && node.id !== "out") {
    const remove = element("button", tr("Retirer le module"), "remove-module");
    remove.onclick = () => {
      patch.modules = patch.modules.filter((item) => item !== node);
      patch.connections = patch.connections.filter(([from, to]) => from !== node.id && to !== node.id);
      if (patch.routings) {
        patch.routings = patch.routings.filter(([from, to]) => from !== node.id && to !== node.id);
        if (!patch.routings.length) delete patch.routings;
      }
      if (patch.modulations) {
        patch.modulations = patch.modulations.filter(([from, to]) => from !== node.id && to !== node.id);
        if (!patch.modulations.length) delete patch.modulations;
      }
      if (patch.midiLearn) {
        patch.midiLearn = patch.midiLearn.filter(([id]) => id !== node.id);
        if (!patch.midiLearn.length) delete patch.midiLearn;
      }
      delete patch.layout?.[node.id];
      if (cableSource === node.id) cableSource = undefined;
      selectedNode = undefined;
      changed();
    };
    details.append(remove);
  }
}

function renderNodeControls(details, node, patch, sliders = false) {
  const spec = nodeSpec(patch, catalog, node);
  if (node.type === "FM4") { renderFm4Controls(details, node, patch, spec, sliders); return; }
  if (node.type === "Arpeggiator") {
    const externalTrigger = patch.routings?.some(([, to, input]) => to === node.id && input === "Trigger");
    details.append(element("p", tr("Les notes tenues pilotent l’arpège. Relie Pitch et Velocity aux oscillateurs. {0}", externalTrigger ? tr("Le câble Trigger détermine la cadence ; Division est désactivée.") :
        tr("Sans câble Trigger, Division règle la cadence à partir de l’horloge générale."))));
  } else if (node.type === "Clock") {
    details.append(element("p", tr("La source et le BPM se règlent dans les réglages du patch.")));
  } else if (node.type === "Delay") {
    details.append(element("p", tr("Mono : répétitions communes ; Stereo : canaux indépendants ; Ping-pong : répétitions alternées gauche/droite. Link lie Time L/R et les divisions. Désactive-le pour les régler séparément. Maximum 2 secondes par canal.")));
  } else if (node.type === "Pan") {
    details.append(element("p", tr("Convertit une source mono en stéréo. Pan déplace le son de gauche à droite à puissance constante ; le centre vaut −3 dB par canal.")));
  } else if (node.type === "Chorus") {
    details.append(element("p", tr("Chorus stéréo : Rate règle la vitesse, Depth la profondeur et Mix le dosage. Une valeur de Rate de 100 correspond à 1 Hz.")));
  } else if (node.type === "Rotary") {
    details.append(element("p", tr("Cabine rotative stéréo à deux rotors. Speed choisit Slow ou Fast ; Transition règle l’inertie. Slow/Fast sont en centièmes de Hz. Depth dose le mouvement.")));
  } else if (node.type === "Stereo") {
    details.append(element("p", tr("Conversion mono → stéréo sans atténuation. L / Mono seul est envoyé aux deux canaux ; branche R pour assembler deux sources mono distinctes.")));
  } else if (node.type === "Mono") {
    details.append(element("p", tr("Conversion stéréo → mono : moyenne de L et R. Permet de rejoindre une chaîne mono.")));
  } else if (node.type === tr("Stereo Mix")) {
    details.append(element("p", tr("Somme jusqu’à trois signaux stéréo. Les réglages In 1–3 agissent sur les deux canaux ; même marge de niveau que Mix.")));
  } else if (node.type === "Osc") {
    details.append(element("p", tr("Unison superpose jusqu’à quatre oscillations désaccordées. Mix dose cette couche avec le son simple ; Level règle l’ensemble.")));
    if (firstOscillator(patch)?.id === node.id)
      details.append(element("p", tr("FM : à 0 le couplage est désactivé. Au-dessus, le deuxième Osc module ce premier Osc, même si son Level est à zéro. Il peut rester sans câble audio.")));
  } else if (node.type === "VCA") {
    details.append(element("p", tr("Relie une enveloppe ou un LFO à Gain. Depth dose son action et Level fixe le niveau maximum. Une CV négative ferme le VCA. Velocity multiplie le gain.")));
  } else if (node.type === "Noise") {
    details.append(element("p", tr("Bruit blanc par voix, avec contrôle du niveau et de la vélocité.")));
  } else if (node.type === "Sub") {
    details.append(element("p", tr("Suit la note et le glide, une ou deux octaves plus bas. Avec un module MIDI ou Arpeggiator, câble son entrée Pitch.")));
  } else if (node.type === tr("Ring Mod")) {
    details.append(element("p", tr("Multiplie les deux entrées audio. Mix dose le résultat avec la première entrée.")));
  } else if (node.type === "Wavefolder") {
    details.append(element("p", tr("Drive replie le signal, Symmetry décale le repliement et Mix dose le résultat. Les trois paramètres sont modulables.")));
  }
  if (sampleModuleTypes.includes(node.type)) renderSampleControl(details, node);
  const oscPreview = node.type === "Osc" ? oscillatorWavePreview(node, patch) : null;
  const updatePreview = sliders ? addModulePreview(details, node, spec) : undefined;
  for (const parameter of spec?.parameters ?? []) {
    if (node.type === "Sampler" && parameter.name === "Root") {
      const names = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
      const choices = Array.from({ length: 128 }, (_, midiNote) => names[midiNote % 12] + (Math.floor(midiNote / 12) - 1));
      details.append(parameterControl(node, { ...parameter, choices }, patch));
      continue;
    }
    if (oscPreview && parameter.name === "Wave") {
      const choices = [...builtinWaves, ...(patch.waves ?? []).map(waveLabel)];
      const waveChanged = (value) => {
        oscPreview.update();
        const id = patch.waves?.[value - builtinWaves.length];
        if (id && !waveLibrary.has(id) && port && catalog.maxWaves && !busy) readRemoteWave(id);
      };
      details.append(parameterControl(node, { ...parameter, max: choices.length - 1, choices },
        patch, "parameters", waveChanged), oscPreview.canvas, oscPreview.fetch);
      oscPreview.update();
    } else {
      details.append(parameterControl(node, parameter, patch, "parameters",
        parameter.name === "PWM" ? oscPreview?.update : updatePreview, sliders));
    }
  }
}

function renderFm4Controls(panel, node, patch, spec, sliders) {
  panel.append(element("p", tr("Quatre opérateurs sinus. Algorithm choisit leur architecture, Depth dose la modulation et Feedback réinjecte l’opérateur 1. Chaque opérateur possède son ratio, son niveau et son ADSR.")));
  const canvas = element("canvas", "", "osc-wave-preview");
  canvas.width=360; canvas.height=160;
  canvas.setAttribute("role", "img"); canvas.setAttribute("aria-label", tr("Architecture des quatre opérateurs FM"));
  function draw() {
    const [inputs, carriers] = fm4Topologies[node.parameters?.Algorithm ?? 4];
    const ctx=canvas.getContext("2d"); ctx.clearRect(0,0,360,160); ctx.lineWidth=2; ctx.strokeStyle="#a99de8";
    const x=[40,130,220,310];
    for(let i=0;i<4;++i) {
      for(let j=0;j<i;++j) if(inputs[i]&(1<<j)) {
        const y=72+18*(i-j);
        ctx.beginPath();ctx.moveTo(x[j],65);ctx.bezierCurveTo(x[j],y,x[i],y,x[i],65);ctx.stroke();
      }
      if(carriers&(1<<i)){ctx.beginPath();ctx.moveTo(x[i],65);ctx.lineTo(x[i],137);ctx.lineTo(340,137);ctx.stroke();}
    }
    ctx.font="bold 18px system-ui";ctx.textAlign="center";
    for(let i=0;i<4;++i){ctx.fillStyle=carriers&(1<<i)?"#75649e":"#303541";ctx.fillRect(x[i]-21,23,42,42);ctx.strokeRect(x[i]-21,23,42,42);ctx.fillStyle="#eeeeff";ctx.fillText(String(i+1),x[i],51);}
    ctx.font="12px system-ui";ctx.fillStyle="#d2c9f5";ctx.fillText("OUT",335,155);
  }
  panel.append(canvas);draw();
  for(const parameter of spec.parameters.filter(p=>!(/^[1-4] /.test(p.name))))
    panel.append(parameterControl(node,parameter,patch,"parameters",parameter.name==="Algorithm"?draw:undefined,sliders));
  for(let i=0;i<4;++i){
    const group=element("details","","fm-operator");group.open=i===0;
    group.append(element("summary",tr("Opérateur {0}", i+1)));
    for(const parameter of spec.parameters.filter(p=>p.name.startsWith(`${i+1} `)))group.append(parameterControl(node,parameter,patch,"parameters",undefined,sliders));
    panel.append(group);
  }
}

function renderSampleControl(panel, node) {
  const row = element("label", "", "parameter");
  row.append(element("span", tr("Sample")));
  const select = element("select", "");
  const empty = element("option", tr("Choisir un sample"));
  empty.value = "";
  select.append(empty);
  for (const sample of sampleList()) {
    const option = element("option", sample.name);
    option.value = sample.id;
    select.append(option);
  }
  if (node.sample && ![...select.options].some((option) => option.value === node.sample)) {
    const option = element("option", node.sampleName ?? node.sample);
    option.value = node.sample;
    select.append(option);
  }
  select.value = node.sample ?? "";
  select.onchange = () => {
    if (select.value) {
      node.sample = select.value;
      node.sampleName = select.selectedOptions[0].textContent;
    } else {
      delete node.sample;
      delete node.sampleName;
    }
    changed();
  };
  row.append(select);
  const library = element("button", tr("Ouvrir Samples"));
  library.onclick = () => openSampleLibrary(true, node.sample);
  const preview = element("canvas", "", "osc-wave-preview");
  preview.width = 320;
  preview.height = 90;
  preview.setAttribute("aria-label", tr("Courbe du sample sélectionné"));
  panel.append(row, preview, library);
  drawSample(preview, sampleLibrary.get(node.sample));
}

function addModulePreview(panel, node, spec) {
  const envelope = node.type === "ADSR" || node.type === "Env";
  if (!envelope && node.type !== "LFO") return undefined;
  const preview = element("canvas", "", "osc-wave-preview");
  preview.width = 320;
  preview.height = 96;
  preview.setAttribute("role", "img");
  preview.setAttribute("aria-label", envelope ? tr("Courbe de l’enveloppe ADSR") : tr("Forme d’onde du LFO"));
  panel.append(preview);
  const value = (name) => node.parameters?.[name] ?? spec.parameters.find((parameter) => parameter.name === name)?.initial;
  function update() {
    if (!envelope) {
      drawWave(preview, undefined, value("Wave"));
      return;
    }
    const durations = [value("Attack"), value("Decay"), 150, value("Release")];
    const total = durations.reduce((sum, duration) => sum + duration, 0);
    const sustain = value("Sustain") / 100;
    const levels = [1, sustain, sustain, 0];
    const ctx = preview.getContext("2d");
    ctx.clearRect(0, 0, preview.width, preview.height);
    let x = 10;
    const baseline = preview.height - 10;
    ctx.beginPath();
    ctx.moveTo(x, baseline);
    durations.forEach((duration, index) => {
      x += duration / total * (preview.width - 20);
      ctx.lineTo(x, baseline - levels[index] * (baseline - 10));
    });
    const fill = ctx.createLinearGradient(0, 10, 0, baseline);
    fill.addColorStop(0, "#b8acff66");
    fill.addColorStop(1, "#b8acff00");
    ctx.fillStyle = fill;
    ctx.fill();
    ctx.strokeStyle = "#b8acff";
    ctx.lineWidth = 2;
    ctx.stroke();
  }
  update();
  return update;
}

function renderParameterPanels() {
  const view = $("parameter-view");
  view.replaceChildren();
  const patch = activeDraft()?.patch;
  if (!patch) return;
  if (catalog.settings) {
    for (const play of [false, true]) {
      const parameters = catalog.settings.parameters.filter(p => playSettings.includes(p.name) === play);
      if (!parameters.length) continue;
      const panel = element("section", "", "panel module-card");
      panel.append(element("h2", play ? tr("Jeu") : tr("Réglages du patch")));
      if (!play) panel.append(patchCategoryControl(patch));
      if (play) panel.append(element("p", tr("Mono : priorité à la dernière note tenue. Legato : glide sur les notes qui se chevauchent. Retrigger relance les enveloppes en mono legato.")));
      for (const parameter of parameters) panel.append(parameterControl(patch, parameter, patch, "settings", undefined, true));
      view.append(panel);
    }
  }
  for (const node of patch.modules) {
    if (!moduleSpec(catalog, node.type)?.parameters?.length) continue;
    const panel = element("section", "", "panel module-card");
    const title = element("h2", tr(node.type));
    title.append(element("small", node.id));
    panel.append(title);
    renderNodeControls(panel, node, patch, true);
    view.append(panel);
  }
}

function render() {
  renderLibrary();
  renderActions();
  if (screen === "editor") {
    if (editorView === "parameters") renderParameterPanels();
    else {
      renderPalette();
      renderGraph();
      renderInspector();
    }
  } else if (screen === "waveforms") {
    renderWaveBrowser();
  } else if (screen === "samples") {
    renderSamples();
  }
}

function changed(redraw = true) {
  persist();
  if (redraw) render();
  else {
    renderLibrary();
    renderActions();
  }
}

async function disconnect() {
  const currentPort = port;
  port = undefined;
  deviceCatalog = undefined;
  currentFirmware = undefined;
  remoteSamples.clear();
  samplePreviewFailures.clear();
  if (screen === "samples") renderSamples();
  canUpload = false;
  if (currentPort) await currentPort.close().catch(() => {});
}

async function perform(label, operation, closeOnError = false) {
  busy = label;
  showError("");
  renderActions();
  try {
    await operation();
  } catch (error) {
    if (closeOnError || !port?.readable || !port?.writable) await disconnect();
    if (error.name !== "NotFoundError") showError(error.message);
  } finally {
    busy = "";
    renderActions();
    if (screen === "samples") queueMicrotask(loadSelectedSamplePreview);
  }
}

function readRemoteWave(id) {
  if (!id || busy || !port) return;
  perform(tr("Lecture de la forme d’onde…"), async () => {
    const wave = await fetchWave(port, id);
    waveLibrary.set(id, wave);
    try {
      await saveWaves([wave]);
    } catch {
      $("wave-message").textContent = tr("L’aperçu est disponible pour cette session.");
    }
    renderInspector();
    if (screen === "waveforms") renderWaveBrowser();
    if (screen === "editor" && editorView === "parameters") renderParameterPanels();
  }).then(() => {
    if (screen === "waveforms") {
      renderWaveDetail();
      if (!$("error").hidden) $("wave-message").textContent = $("error").textContent;
    }
  });
}

function withKeys(items) {
  return items.map((item) => ({ ...item, key: item.file, patch: normalizePatch(item.patch) }));
}

async function readDevice() {
  const result = await fetchPatches(port);
  const latest = withKeys(result.entries);
  deviceCatalog = result.catalog;
  currentFirmware = result.catalog?.firmware;
  // A connected unit determines the release channel. Never retain the other
  // model's cached file after changing devices.
  if (currentFirmware && firmwareRelease?.model !== currentFirmware.model) {
    firmwareRelease = firmwareImage = undefined; firmwareManual = false;
    try { firmwareRelease = await fetchFirmwareRelease(fetch, currentFirmware.model); }
    catch (error) { $("firmware-latest").textContent = error.message; }
  }
  if (currentFirmware?.pendingVerify) {
    $("firmware-message").textContent = tr("Le Modulox vérifie son premier démarrage. Clique sur Vérifier les mises à jour dans quelques secondes.");
  }
  samplePreviewFailures.clear();
  remoteSamples.clear();
  if (deviceCatalog?.sampleLibrary) {
    for (const sample of await fetchSampleLibrary(port)) remoteSamples.set(sample.id, sample);
  }
  const received = pitchCatalog(result.catalog ?? offlineCatalog);
  const missing = [...sampleModuleTypes, ...timingModuleTypes, ...vaModuleTypes, ...stereoModuleTypes, "FM4"].filter((type) => !moduleSpec(received, type));
  catalog = { ...received, modules: [...received.modules, ...missing.map((type) => moduleSpec(offlineCatalog, type))] };
  if (!received.vaExpansion) {
    catalog.modules = catalog.modules.map(spec => spec.type !== "Osc" ? spec : { ...spec, parameters:
      [...spec.parameters, ...moduleSpec(offlineCatalog, "Osc").parameters.filter(p => !spec.parameters.some(q => q.name === p.name))] });
    catalog.settings = { parameters: [...(received.settings?.parameters ?? []),
      ...offlineCatalog.settings.parameters.filter(p => playSettings.includes(p.name) && !received.settings?.parameters.some(q => q.name === p.name))] };
  }
  if (!received.stereoGraph) {
    catalog.modules = catalog.modules.map(spec => spec.type === "Delay" ? moduleSpec(offlineCatalog, "Delay") : spec);
    catalog.output = offlineCatalog.output;
  }
  if (!received.fm4) catalog.modules = catalog.modules.map(spec => spec.type !== "Osc" ? spec : {
    ...spec, parameters: [...spec.parameters, ...moduleSpec(offlineCatalog,"Osc").parameters.filter(p=>p.name==="FM"&&!spec.parameters.some(q=>q.name==="FM"))],
    modulationInputs: [...new Set([...(spec.modulationInputs??[]),"FM"])] });
  canUpload = Boolean(result.catalog);
  entries = latest;
  hasRead = true;
  if (selection?.kind !== "draft" && (!current() || selection.kind === "example")) {
    if (entries.length) select("usb", entries[0].key);
    else selection = undefined;
  }
  render();
}

$("new-patch").onclick = () => {
  if (createDraft()) $("name").select();
};
$("nav-patches").onclick = $("back-patches").onclick = () => showScreen("patches");
function resumeDraft() {
  if (!drafts.length) return;
  if (!activeDraft()) select("draft", drafts[0].key);
  showScreen("editor");
}
$("nav-editor").onclick = $("back-editor").onclick = resumeDraft;
$("copy").onclick = () => activeDraft() ? resumeDraft() : createDraft(current());
$("tab-graph").onclick = () => switchEditorView("graph");
$("tab-parameters").onclick = () => switchEditorView("parameters");
$("discard-draft").onclick = () => {
  if (!confirm(tr("Fermer ce patch en cours ? Les modifications non sauvegardées seront perdues."))) return;
  drafts = [];
  persist();
  const items = hasRead ? entries : examples;
  selection = items.length ? { kind: hasRead ? "usb" : "example", key: items[0].key } : undefined;
  showScreen("patches");
};
$("settings").onclick = () => selectNode("$settings");
async function openSamples(forPatch) {
  await waveLibraryReady;
  samplesForPatch = forPatch;
  previewId = undefined;
  $("wave-search").value = "";
  $("wave-scope").value = patchWaveIds().length ? "patch" : "library";
  $("wave-scope").hidden = !forPatch;
  $("back-editor").hidden = !forPatch;
  showScreen("waveforms");
}
$("nav-waveforms").onclick = () => openSamples(false);
$("wave-library").onclick = () => openSamples(true);

function sampleList() {
  const items = new Map(sampleLibrary);
  for (const sample of remoteSamples.values()) items.set(sample.id, { ...items.get(sample.id), ...sample });
  for (const { patch } of entries) {
    for (const node of patch.modules) {
      if (sampleModuleTypes.includes(node.type) && node.sample && !items.has(node.sample)) {
        items.set(node.sample, { id: node.sample, name: node.sampleName ?? node.sample });
      }
    }
  }
  return [...items.values()].sort((a, b) => a.name.localeCompare(b.name, undefined, { numeric: true }));
}

function renderSamples() {
  const list = $("sample-list");
  const items = sampleList();
  if (!items.some((item) => item.id === selectedSample)) selectedSample = items[0]?.id;
  $("sample-count").textContent = tr("{0} en local{1}", sampleLibrary.size, port && deviceCatalog?.sampleLibrary ? tr(" · {0} sur le Modulox", remoteSamples.size) : "");
  $("sample-refresh").disabled = !port || !deviceCatalog?.sampleLibrary || Boolean(busy);
  list.replaceChildren();
  if (!items.length) list.append(element("p", tr("Importe un WAV ou les démos de Sampbrain2."), "empty-state"));
  for (const item of items) {
    const row = element("div", "", "sample-row");
    const select = element("button", item.name);
    select.append(element("small", remoteSamples.has(item.id) ? (item.pcm ? tr("Local + Modulox") : "Modulox") : tr("Local")));
    select.setAttribute("aria-pressed", String(selectedSample === item.id));
    select.onclick = () => {
      selectedSample = item.id;
      samplePreviewFailures.delete(item.id);
      renderSamples();
      loadSelectedSamplePreview();
    };
    row.append(select);
    if (item.pcm) {
      const remove = element("button", "×", "sample-remove");
      remove.setAttribute("aria-label", tr("Supprimer {0} de la bibliothèque locale", item.name));
      remove.onclick = async () => {
        if (!confirm(tr("Supprimer « {0} » de cette bibliothèque locale ?", item.name))) return;
        try {
          await deleteSample(item.id);
          sampleLibrary.delete(item.id);
          if (item.id === selectedSample) samplePlayback?.stop();
          renderSamples();
        } catch {
          $("sample-message").textContent = tr("Impossible de supprimer ce sample.");
        }
      };
      row.append(remove);
    }
    list.append(row);
  }
  renderSampleDetail();
}

function renderSampleDetail() {
  const sample = sampleList().find((item) => item.id === selectedSample);
  $("sample-name").textContent = sample?.name ?? tr("Aperçu");
  $("sample-info").textContent = sample?.pcm
    ? (sample.pcm.byteLength / (2 * sampleRate)).toFixed(2) + " s · mono · 44,1 kHz"
    : sample ? tr("{0}Disponible sur le Modulox. Récupère-le pour l’écouter ou l’exporter.", sample.bytes ? (sample.bytes / (2 * sampleRate)).toFixed(2) + " s · " : "")
      : tr("WAV mono ou stéréo, convertis en mono à l’import.");
  $("sample-play").disabled = !sample?.pcm;
  $("sample-send").disabled = !sample?.pcm || !port || !deviceCatalog?.maxSampleBytes || Boolean(busy);
  $("sample-fetch").hidden = !sample || Boolean(sample.pcm);
  $("sample-fetch").disabled = !port || !deviceCatalog?.maxSampleBytes || Boolean(busy);
  $("sample-refresh").disabled = !port || !deviceCatalog?.sampleLibrary || Boolean(busy);
  $("sample-export").disabled = !sample?.pcm;
  $("sample-delete-device").disabled = !sample || !port || !remoteSamples.has(sample.id) ||
    !deviceCatalog?.sampleDeletion || Boolean(busy);
  $("sample-delete-device").title = port && !deviceCatalog?.sampleDeletion ? tr("Mets à jour le firmware pour supprimer les samples.") : tr("Supprimer uniquement de la flash du Modulox");
  $("sample-label").disabled = !sample || Boolean(busy);
  if (document.activeElement !== $("sample-label")) $("sample-label").value = sample?.name ?? "";
  $("sample-rename").disabled = !sample || Boolean(busy) ||
    (!sample.pcm && (!remoteSamples.has(sample.id) || !deviceCatalog?.sampleNames));
  const cached = samplePreviewCache.get(sample?.id);
  drawSample($("sample-preview"), sample?.pcm ? sample : cached?.pcm ? cached : cached ? { preview: cached } : undefined);
}

function loadSelectedSamplePreview() {
  const sample = sampleList().find((item) => item.id === selectedSample);
  if (screen !== "samples" || !sample || sample.pcm || samplePreviewCache.has(sample.id) ||
      samplePreviewFailures.has(sample.id) || !port || !deviceCatalog?.maxSampleBytes || busy) return;
  const connection = port;
  perform(tr("Chargement de l’aperçu…"), async () => {
    try {
      const preview = deviceCatalog?.samplePreview ? await fetchSamplePreview(connection, sample.id)
        : await fetchSample(connection, sample.id, sample.name);
      if (port !== connection) return;
      samplePreviewCache.set(sample.id, preview);
      while (samplePreviewCache.size > 8) samplePreviewCache.delete(samplePreviewCache.keys().next().value);
      if (screen === "samples") renderSampleDetail();
    } catch (error) {
      samplePreviewFailures.add(sample.id);
      throw error;
    }
  });
}

async function openSampleLibrary(fromEditor = false, id) {
  await samplesReady;
  if (id) selectedSample = id;
  $("sample-back").hidden = !fromEditor;
  showScreen("samples");
  if (port && deviceCatalog?.sampleLibrary && !busy) refreshSampleLibrary();
  else loadSelectedSamplePreview();
}

function refreshSampleLibrary() {
  if (!port || busy || !deviceCatalog?.sampleLibrary) return;
  return perform(tr("Lecture de la bibliothèque…"), async () => {
    const items = await fetchSampleLibrary(port);
    samplePreviewFailures.clear();
    remoteSamples.clear();
    for (const sample of items) remoteSamples.set(sample.id, sample);
    renderSamples();
    $("sample-message").textContent = tr("{0} samples dans la flash du Modulox.", items.length);
  });
}

async function importSamples(files) {
  let imported = 0;
  const rejected = [];
  for (const file of files) {
    try {
      const sample = await readSample(file);
      await saveSample(sample);
      sampleLibrary.set(sample.id, sample);
      selectedSample = sample.id;
      imported++;
    } catch (error) {
      rejected.push(file.name + " (" + error.message + ")");
    }
  }
  $("sample-message").textContent = tr("{0} WAV importés{1}", imported, rejected.length ? tr(" · Échec : {0}", rejected.join(", ")) : "");
  if (screen === "samples") renderSamples();
}

function sampleProgress(sent, total) {
  $("status").textContent = tr("Transfert du sample {0} %", Math.round(sent / total * 100));
}

$("nav-samples").onclick = () => openSampleLibrary();
$("sample-refresh").onclick = refreshSampleLibrary;
$("sample-back").onclick = resumeDraft;
$("sample-import").onclick = () => $("sample-files").click();
$("sample-files").onchange = async (event) => {
  const files = [...event.target.files];
  event.target.value = "";
  if (files.length) await importSamples(files);
};
$("sample-demos").onclick = async () => {
  const button = $("sample-demos");
  button.disabled = true;
  try {
    const files = await Promise.all(["kick01.wav", "snare02.wav", "kick04.wav", "voice.wav"].map(async (name) => {
      const response = await fetch("./demo-samples/" + name);
      if (!response.ok) throw new Error(name);
      return new File([await response.blob()], name, { type: "audio/wav" });
    }));
    await importSamples(files);
  } catch {
    $("sample-message").textContent = tr("Impossible de lire les démos Sampbrain2.");
  } finally {
    button.disabled = false;
  }
};
$("sample-play").onclick = async () => {
  const sample = sampleLibrary.get(selectedSample);
  if (!sample) return;
  try {
    samplePlayback?.stop();
    audioContext ??= new AudioContext();
    await audioContext.resume();
    const pcm = new DataView(sample.pcm.buffer, sample.pcm.byteOffset, sample.pcm.byteLength);
    const buffer = audioContext.createBuffer(1, pcm.byteLength / 2, sampleRate);
    const frames = buffer.getChannelData(0);
    for (let i = 0; i < frames.length; i++) frames[i] = pcm.getInt16(i * 2, true) / 32768;
    const source = audioContext.createBufferSource();
    source.buffer = buffer;
    source.connect(audioContext.destination);
    source.onended = () => { if (samplePlayback === source) samplePlayback = undefined; };
    samplePlayback = source;
    source.start();
  } catch {
    $("sample-message").textContent = tr("Impossible de lire le sample dans ce navigateur.");
  }
};
$("sample-send").onclick = () => {
  const sample = sampleLibrary.get(selectedSample);
  if (!sample || !port || busy) return;
  perform(tr("Envoi du sample…"), async () => {
    await uploadSample(port, sample, sampleProgress, deviceCatalog?.sampleNames);
    if (deviceCatalog?.sampleLibrary) remoteSamples.set(sample.id, { id: sample.id, name: sample.name, bytes: sample.pcm.byteLength });
    renderSamples();
    $("sample-message").textContent = tr("{0} disponible dans la flash du Modulox.", sample.name);
  });
};

$("sample-rename").onclick = () => {
  const sample = sampleList().find((item) => item.id === selectedSample);
  const entered = $("sample-label").value.trim();
  if (!sample || !entered || busy) return;
  perform(tr("Renommage du sample…"), async () => {
    const remote = port && remoteSamples.has(sample.id) && deviceCatalog?.sampleNames;
    const name = remote ? await renameSample(port, sample.id, entered) : entered.slice(0, 31);
    if (remote) remoteSamples.set(sample.id, { ...remoteSamples.get(sample.id), name });
    if (sample.pcm) {
      const local = { ...sampleLibrary.get(sample.id), name };
      sampleLibrary.set(sample.id, local);
      await saveSample(local);
    }
    for (const { patch } of [...entries, ...drafts]) for (const node of patch.modules)
      if (node.sample === sample.id) node.sampleName = name;
    persist();
    renderSamples();
    $("sample-message").textContent = remote ? tr("Nom enregistré sur le Modulox.") : tr("Nom enregistré dans la bibliothèque locale.");
  });
};

$("sample-export").onclick = () => {
  const sample = sampleList().find((item) => item.id === selectedSample);
  if (!sample?.pcm) return;
  const url = URL.createObjectURL(new Blob([sampleWav(sample)], { type: "audio/wav" }));
  const link = element("a"); link.href = url;
  link.download = (sample.name.replace(/[<>:"/\\|?*\x00-\x1f]/g, "_") || "sample") + ".wav";
  document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
};
$("sample-fetch").onclick = () => {
  const sample = sampleList().find((item) => item.id === selectedSample);
  if (!sample || !port || busy) return;
  perform(tr("Lecture du sample…"), async () => {
    const cached = samplePreviewCache.get(sample.id);
    const local = cached?.pcm ? { ...cached, id: sample.id, name: sample.name } : await fetchSample(port, sample.id, sample.name);
    await saveSample(local);
    sampleLibrary.set(local.id, local);
    if (screen === "samples") renderSamples();
  });
};

$("sample-delete-device").onclick = () => {
  const sample = sampleList().find((item) => item.id === selectedSample);
  if (!sample || !port || busy || !deviceCatalog?.sampleDeletion) return;
  const localUse = [...drafts, ...savedPatches].some(({ patch }) => patch.modules.some((node) => node.sample === sample.id));
  const keepLocal = localUse && !sampleLibrary.has(sample.id);
  if (!confirm(tr("Supprimer « {0} » de la flash du Modulox ?{1}", sample.name, keepLocal ? tr(" Un patch local l’utilise : une copie locale sera conservée pour pouvoir le renvoyer.") : ""))) return;
  perform(tr("Suppression du sample…"), async () => {
    if (keepLocal) {
      const cached = samplePreviewCache.get(sample.id);
      const local = cached?.pcm ? { ...cached, id: sample.id, name: sample.name } : await fetchSample(port, sample.id, sample.name);
      await saveSample(local); sampleLibrary.set(sample.id, local);
    }
    await deleteDeviceSample(port, sample.id);
    remoteSamples.delete(sample.id);
    samplePreviewCache.delete(sample.id); samplePreviewFailures.delete(sample.id);
    samplePlayback?.stop();
    renderSamples();
    $("sample-message").textContent = tr("Sample supprimé du Modulox.{0}", sampleLibrary.has(sample.id) ? tr(" Sa copie locale est conservée ; la croix permet de la supprimer aussi.") : "");
  });
};

$("wave-scope").onchange = renderWaveBrowser;
$("wave-search").oninput = renderWaveBrowser;
$("wave-import").onclick = () => $("wave-files").click();
$("wave-fetch").onclick = () => readRemoteWave(previewId);
$("wave-add").onclick = () => {
  const ids = patchWaveIds();
  setBank([...ids, ...waveResults().filter((id) => !ids.includes(id)).slice(0, maxWaves() - ids.length)]);
};
$("wave-files").onchange = async (event) => {
  const files = [...event.target.files];
  if (!files.length) return;
  const importButton = $("wave-import");
  importButton.disabled = true;
  try {
    const imported = [];
    let rejected = 0;
    for (const file of files) {
      try { imported.push(await readWave(file)); }
      catch { rejected++; }
    }
    let added = 0;
    for (const wave of imported) {
      if (!waveLibrary.has(wave.id)) added++;
      waveLibrary.set(wave.id, wave);
    }
    $("wave-message").textContent = tr("{0} ajoutées{1}", added, rejected ? tr(" · {0} WAV incompatibles", rejected) : "");
    try {
      await saveWaves(imported);
    } catch {
      $("wave-message").textContent += tr(" · Bibliothèque conservée pour cette session seulement.");
    }
    $("wave-scope").value = "library";
    $("wave-search").value = "";
    previewId = imported[0]?.id;
    renderWaveBrowser();
    renderInspector();
  } finally {
    event.target.value = "";
    importButton.disabled = false;
  }
};
$("name").oninput = () => {
  activeDraft().patch.name = $("name").value;
  changed(false);
};
$("arrange").onclick = () => {
  const patch = activeDraft().patch;
  patch.layout = layout(patch);
  changed();
};
$("connect").onclick = () => perform(tr("Connexion USB…"), async () => {
  if (port) return disconnect();
  const selected = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x4009 }] });
  await selected.open({ baudRate: 115200 });
  port = selected;
  await port.setSignals({ dataTerminalReady: true });
  await readDevice();
  if (expectedFirmware && currentFirmware) {
    currentFirmware = await fetchFirmwareInfo(port);
    verifyFirmwareRestart();
  }
}, true);
$("refresh").onclick = () => perform(tr("Téléchargement USB…"), readDevice, true);
$("upload").onclick = () => {
  const draft = activeDraft();
  const unavailable = patchSampleError(draft.patch);
  if (unavailable) return showError(unavailable);
  const error = validatePatch(draft.patch, catalog);
  if (error) return showError(error);
  perform(tr("Envoi en flash…"), async () => {
    if (draft.targetFile) {
      await readDevice();
      if (!entries.some((entry) => entry.file === draft.targetFile)) {
        throw new Error(tr("Le patch d’origine a été supprimé. Sauvegarde-le localement puis recharge-le pour créer un nouveau patch."));
      }
    }
    await Promise.all([waveLibraryReady, samplesReady]);
    const patch = { ...structuredClone(draft.patch), name: draft.patch.name.trim() };
    for (const node of patch.modules) if (remoteSamples.has(node.sample)) node.sampleName = remoteSamples.get(node.sample).name;
    const id = await uploadPatch(port, draft.key, patchForDevice(patch, catalog),
      catalog.maxBytes, catalog.maxPatches, waveLibrary, (sent, total) => {
        $("status").textContent = tr("Formes d’onde {0}/{1}", sent, total);
      }, sampleLibrary, sampleProgress, deviceCatalog?.sampleNames);
    if (draft.patch.name.trim() === patch.name) draft.patch.name = patch.name;
    draft.lastSent = serialized(patch);
    const file = draft.targetFile ?? draft.key + ".json";
    entries = entries.filter((entry) => entry.file !== file);
    entries.push({ id, file, key: file, patch });
    entries.sort((a, b) => a.id - b.id);
    drafts = [];
    persist();
    select("usb", file);
    $("home-message").textContent = tr("Patch enregistré sur le Modulox.");
    showScreen("patches");
  });
};
$("modules-toggle").onclick = () => {
  const open = $("module-panel").hidden;
  $("module-panel").hidden = !open;
  $("modules-toggle").setAttribute("aria-expanded", String(open));
};
async function fileOperation(label, operation) {
  if (busy) return;
  busy = label; showError(""); renderActions();
  try { return await operation(); }
  catch (error) { if (error.name !== "AbortError") showError(error.message); }
  finally { busy = ""; renderActions(); }
}
async function accessPatchFolder(change = false) {
  await folderReady;
  if (!window.showDirectoryPicker) throw new Error(tr("Le choix d’un dossier nécessite Chrome ou Edge. Utilise Exporter un fichier pour une sauvegarde sur cet ordinateur."));
  if (change || !patchFolder) {
    const handle = await window.showDirectoryPicker({ id: "modulox-patches", mode: "readwrite", startIn: "documents" });
    patchFolder = handle;
    // Failure to remember access does not prevent saving to the chosen folder.
    await folderPreference(handle).catch(() => {});
  } else if (await patchFolder.queryPermission({ mode: "readwrite" }) !== "granted" &&
      await patchFolder.requestPermission({ mode: "readwrite" }) !== "granted") {
    throw new Error(tr("Autorise l’accès au dossier ou choisis un autre dossier de patches."));
  }
  return patchFolder;
}
async function ensurePersonalAssets(patch) {
  await Promise.all([waveLibraryReady, samplesReady]);
  for (const id of patch.waves ?? []) if (!waveLibrary.has(id)) {
    if (!port) throw new Error(tr("Forme d’onde absente : connecte le Modulox pour la récupérer avant de sauvegarder ({0}).", id));
    const wave = await fetchWave(port, id); await saveWaves([wave]); waveLibrary.set(id, wave);
  }
  const samples = new Map(patch.modules.filter(n => sampleModuleTypes.includes(n.type) && n.sample).map(n => [n.sample, n.sampleName]));
  for (const [id, name] of samples) if (!sampleLibrary.get(id)?.pcm) {
    if (!port) throw new Error(tr("Sample absent : connecte le Modulox pour le récupérer avant de sauvegarder ({0}).", id));
    const sample = await fetchSample(port, id, name ?? remoteSamples.get(id)?.name);
    await saveSample(sample); sampleLibrary.set(id, sample);
  }
}
async function openPersonalFile(file) {
  const imported = await fileOperation(tr("Ouverture du fichier…"), async () => {
    await Promise.all([waveLibraryReady, samplesReady]);
    const result = await readPatchFile(file, { waves: waveLibrary, samples: sampleLibrary });
    // Validate the whole file before touching the current patch or libraries.
    if (result.waves.length) await saveWaves(result.waves);
    for (const sample of result.samples) await saveSample(sample);
    for (const wave of result.waves) waveLibrary.set(wave.id, wave);
    for (const sample of result.samples) sampleLibrary.set(sample.id, sample);
    return result;
  });
  if (imported && createDraft({ patch: imported.patch })) {
    $("local-dialog").close();
    $("hint").textContent = tr("Fichier ouvert sur cet ordinateur. Enregistrer dans mon dossier crée une nouvelle copie complète.");
  }
}
async function showFolderPatches() {
  const list = $("folder-list"); list.replaceChildren();
  if (!patchFolder) { $("folder-status").textContent = tr("Choisis un dossier pour retrouver tes fichiers de patches."); return; }
  $("folder-status").textContent = tr("Dossier : {0} · Fichiers personnels, jamais publiés sur le serveur.", patchFolder.name);
  if (await patchFolder.queryPermission({ mode: "readwrite" }) !== "granted") {
    list.append(element("p", tr("Clique sur Actualiser pour autoriser de nouveau l’accès à ce dossier."))); return;
  }
  const handles = [];
  for await (const entry of patchFolder.values()) if (entry.kind === "file" && entry.name.endsWith(".modulox.json")) handles.push(entry);
  handles.sort((a, b) => a.name.localeCompare(b.name));
  if (!handles.length) list.append(element("p", tr("Aucun patch enregistré dans ce dossier.")));
  for (const handle of handles) {
    const row = element("div", "", "local-row");
    const label = handle.name.replace(/(?:-[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12})?\.modulox\.json$/i, "").replace(/-/g, " ");
    const open = element("button", label);
    open.title = tr("Ouvrir {0}", handle.name);
    open.onclick = async () => { if (!busy) try { await openPersonalFile(await handle.getFile()); } catch (error) { showError(error.message); } };
    row.append(open); list.append(row);
  }
}
$("choose-folder").onclick = () => fileOperation(tr("Choix du dossier…"), async () => { await accessPatchFolder(true); await showFolderPatches(); });
$("refresh-folder").onclick = () => fileOperation(tr("Lecture du dossier…"), async () => { await accessPatchFolder(); await showFolderPatches(); });
$("save-local").onclick = $("save-home").onclick = async () => {
  const item = current(); if (!item || busy) return;
  const patch = structuredClone(item.patch);
  const filename = await fileOperation(tr("Enregistrement du fichier…"), async () => {
    const directory = await accessPatchFolder();
    await ensurePersonalAssets(patch);
    const contents = await packPatch(patch, { waves: waveLibrary, samples: sampleLibrary });
    return writePatchFile(directory, contents, patch.name);
  });
  if (filename) $(screen === "editor" ? "hint" : "home-message").textContent = tr("Fichier enregistré dans {0} : {1}", patchFolder.name, filename);
};
$("export-patch").onclick = $("export-home").onclick = async () => {
  const item = current(); if (!item || busy) return;
  const patch = structuredClone(item.patch);
  await fileOperation(tr("Export du fichier…"), async () => {
    await ensurePersonalAssets(patch);
    const contents = await packPatch(patch, { waves: waveLibrary, samples: sampleLibrary });
    const url = URL.createObjectURL(new Blob([contents], { type: "application/json" }));
    const link = element("a"); link.href = url;
    link.download = (patch.name.replace(/[<>:"/\\|?*\x00-\x1f]/g, "_") || "patch") + ".modulox.json";
    document.body.append(link); link.click(); link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });
};
$("import-patch").onclick = () => { if (!busy) $("patch-file").click(); };
$("patch-file").onchange = async () => {
  const file = $("patch-file").files[0]; $("patch-file").value = "";
  if (file && !busy) await openPersonalFile(file);
};
function showLocalPatches() {
  const list = $("local-list");
  list.replaceChildren();
  $("legacy-patches").hidden = !savedPatches.length;
  if (!savedPatches.length) list.append(element("p", tr("Aucun patch sauvegardé dans ce navigateur.")));
  for (const item of savedPatches) {
    const row = element("div", "", "local-row");
    const button = element("button", item.patch.name);
    button.onclick = () => {
      if (createDraft(item)) $("local-dialog").close();
    };
    const remove = element("button", tr("Supprimer"));
    remove.onclick = () => {
      if (!confirm(tr("Supprimer cette sauvegarde locale ?"))) return;
      const next = savedPatches.filter((p) => p !== item);
      try {
        localStorage.setItem("modulox.saved.v1", JSON.stringify(next));
        savedPatches = next;
        showLocalPatches();
      } catch { showError(tr("Impossible de supprimer la sauvegarde locale.")); }
    };
    row.append(button, remove);
    list.append(row);
  }
}
async function showServerPatches() {
  const list = $("server-patch-list");
  list.replaceChildren(element("p", tr("Chargement des presets d’usine…")));
  try {
    const items = await loadFactoryBank();
    list.replaceChildren();
    if (!items.length) list.append(element("p", tr("Aucun preset d’usine disponible.")));
    for (const item of items) {
      const row = element("div", "", "local-row");
      const button = element("button", item.patch.name);
      button.onclick = async () => {
        button.disabled = true;
        button.textContent = tr("Chargement…");
        const message = element("p", "");
        try {
          await Promise.all([waveLibraryReady, samplesReady]);
          await loadPresetAssets(item.patch, { waves: waveLibrary, samples: sampleLibrary, saveWaves, saveSample });
          if (createDraft({ patch: item.patch })) $("local-dialog").close();
        } catch (error) { message.textContent = error.message; row.after(message); }
        finally { button.disabled = false; button.textContent = item.patch.name; }
      };
      row.append(button);
      list.append(row);
    }
  } catch (error) { list.replaceChildren(element("p", error.message)); }
}
$("load-local").onclick = () => {
  $("local-title").textContent = tr("Mes patches");
  $("local-description").textContent = tr("Des fichiers dans ton dossier, avec les formes d’onde et samples nécessaires. Aucune publication sur le serveur.");
  $("factory-bank").hidden = true; $("personal-bank").hidden = false;
  showLocalPatches();
  folderReady.then(showFolderPatches).catch(error => showError(error.message));
  $("local-dialog").showModal();
};
$("load-factory").onclick = () => {
  $("local-title").textContent = tr("Presets d’usine");
  $("local-description").textContent = tr("La banque fournie avec Modulox. Chaque preset s’ouvre comme une copie personnelle à modifier.");
  $("factory-bank").hidden = false; $("personal-bank").hidden = true;
  $("factory-upload-status").textContent = port ? tr("Banque à 85 %. Les copies déjà envoyées par ce bouton seront réinstallées si nécessaire.") : tr("Connecte le Modulox pour envoyer toute la banque.");
  showServerPatches();
  $("local-dialog").showModal();
};
$("factory-upload-stop").onclick = () => {
  factoryAbort?.abort();
  $("factory-upload-status").textContent = tr("Arrêt demandé : le patch en cours sera terminé.");
};
$("local-dialog").addEventListener("close", () => factoryAbort?.abort());
$("factory-upload-all").onclick = () => {
  if (busy || !port || !canUpload) return;
  perform(tr("Préparation de la banque d’usine…"), async () => {
    const controller = new AbortController();
    factoryAbort = controller;
    renderActions();
    const status = $("factory-upload-status"), progress = $("factory-upload-progress");
    progress.hidden = true;
    try {
      await readDevice();
      const items = await loadFactoryBank();
      const plan = await planFactoryUpload(items, entries, deviceCatalog);
      if (controller.signal.aborted) { status.textContent = tr("Envoi annulé."); return; }
      if (!plan.added && !plan.updated) {
        status.textContent = tr("{0} presets déjà à jour sur le Modulox.", items.length);
        return;
      }
      if (!confirm(tr("Charger la banque d’usine à 85 % ?\n{0} ajout(s), {1} mise(s) à jour, {2} déjà à jour.\nLes copies dont le son a été modifié sont conservées. Les presets non modifiés sont mis à jour avec le volume à 85 %.", plan.added, plan.updated, plan.unchanged))) {
        status.textContent = tr("Envoi annulé.");
        return;
      }
      await Promise.all([waveLibraryReady, samplesReady]);
      progress.max = items.length; progress.value = 0; progress.hidden = false;
      const result = await uploadFactoryBank(plan, {
        port, catalog: deviceCatalog, signal: controller.signal,
        libraries: { waves: waveLibrary, samples: sampleLibrary, saveWaves, saveSample },
        progress({ phase, item, done, total }) {
          const text = phase === "prepare" ? tr("Préparation des ressources : {0}", item.patch.name)
            : `${done}/${total} · ${item.patch.name}`;
          status.textContent = text; $("status").textContent = text;
          progress.value = done;
        },
        onSaved(entry) { entries = [...entries.filter(item => item.file !== entry.file), entry].sort((a, b) => a.id - b.id); },
      });
      const message = result.cancelled
        ? tr("Arrêté : {0} patch(s) enregistré(s), {1}/{2} traités. Relance pour terminer.", result.saved, result.done, items.length)
        : tr("{0} presets d’usine à jour sur le Modulox, volume 85 %.", items.length);
      status.textContent = $("home-message").textContent = message;
    } catch (error) {
      status.textContent = error.message;
      throw error;
    } finally {
      factoryAbort = undefined;
      if (port?.readable && port?.writable) await readDevice().catch(() => {});
    }
  });
};
$("delete-patch").onclick = () => {
  const item = current();
  if (!item || !confirm(tr("Supprimer « {0} » du Modulox ? Recharge un patch sur le synthé avant de sauvegarder depuis ses menus.", item.patch.name))) return;
  perform("Suppression…", async () => {
    await deletePatch(port, item.file);
    if (drafts[0]?.targetFile === item.file) {
      drafts[0].key = "u" + crypto.randomUUID().replaceAll("-", "").slice(0, 13);
      delete drafts[0].targetFile;
      persist();
    }
    await readDevice();
  });
};
navigator.serial?.addEventListener("disconnect", async (event) => {
  if (event.target !== port) return;
  await disconnect();
  renderActions();
});
function renderFirmware() {
  $("firmware-current").textContent = currentFirmware
    ? tr("Installé : {0} · {1} · {2}", currentFirmware.version, (() => { try { return boardForModel(currentFirmware.model).label; } catch { return tr("Modèle inconnu"); } })(), currentFirmware.pendingVerify ? tr("vérification de démarrage") : tr("démarrage confirmé"))
    : port ? tr("Firmware ancien : version non exposée. Une migration initiale A/B est nécessaire.") : tr("Connecte le Modulox pour lire sa version.");
  const older = currentFirmware && firmwareRelease && compareVersions(firmwareRelease.version, currentFirmware.version) < 0;
  if (firmwareRelease) $("firmware-latest").textContent = `Disponible : ${firmwareRelease.version}` + (older ? tr(" · Plus ancienne que le firmware installé") : "") +
    (currentFirmware && compareVersions(firmwareRelease.version, currentFirmware.version) === 0 && firmwareRelease.buildId === currentFirmware.buildId ? tr(" · À jour") : "");
  $("firmware").disabled = Boolean(busy) || !port || !currentFirmware?.usbUpdate || currentFirmware.pendingVerify || !firmwareRelease || currentFirmware.model !== firmwareRelease.model || older && !firmwareManual;
  $("firmware").textContent = currentFirmware?.version === firmwareRelease?.version ? tr("Réinstaller cette version") : tr("Mettre à jour");
  $("firmware-rollback").disabled = Boolean(busy) || !port || !currentFirmware?.canRollback || currentFirmware.pendingVerify;
  $("firmware-check").disabled = $("firmware-file-button").disabled = Boolean(busy);
  $("firmware-recovery").disabled = Boolean(busy) || !navigator.serial || !firmwareRelease;
  $("firmware-factory").disabled = Boolean(busy) || !navigator.serial;
}

function renderFactoryInstallation() {
  const ready = Boolean(factoryInstallSession) && !factoryInstallWorking;
  $("factory-install-consent").disabled = !ready;
  $("factory-install-close").disabled = factoryInstallWorking;
  $("factory-install-prepare").disabled = factoryInstallWorking || !navigator.serial;
  $("factory-install-start").disabled = !ready || !factoryConsentValid($("factory-install-ack").checked, $("factory-install-phrase").value);
}
function clearFactoryConsent() {
  $("factory-install-ack").checked = false;
  $("factory-install-phrase").value = "";
  renderFactoryInstallation();
}
function reportFactoryInstallation(message, percent) {
  $("factory-install-status").textContent = message;
  if (percent !== undefined) $("factory-install-progress").value = percent;
}
async function closeFactorySession() {
  const session = factoryInstallSession;
  factoryInstallSession = undefined;
  clearFactoryConsent();
  await session?.close().catch(() => {});
}
$("firmware-factory").onclick = () => {
  if (busy) return;
  ++factoryInstallGeneration;
  factoryInstallRelease = undefined;
  clearFactoryConsent();
  $("factory-install-release").textContent = tr("L’image usine sera choisie automatiquement après identification du modèle connecté.");
  $("factory-install-device").textContent = tr("Aucune carte identifiée. Aucune écriture effectuée.");
  $("factory-install-status").textContent = "";
  $("factory-install-progress").hidden = true;
  $("factory-install-dialog").showModal();
  renderFactoryInstallation();
};
$("factory-install-ack").onchange = $("factory-install-phrase").oninput = renderFactoryInstallation;
$("factory-install-close").onclick = () => { if (!factoryInstallWorking) $("factory-install-dialog").close(); };
$("factory-install-dialog").addEventListener("cancel", event => { if (factoryInstallWorking) event.preventDefault(); });
$("factory-install-dialog").addEventListener("close", () => { ++factoryInstallGeneration; closeFactorySession(); });
$("factory-install-prepare").onclick = async () => {
  if (busy || factoryInstallWorking) return;
  // Port selection is directly triggered by the click, preserving Web Serial user activation.
  const selected = navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }] });
  factoryInstallWorking = true;
  clearFactoryConsent();
  $("factory-install-progress").hidden = true;
  try {
    await fileOperation(tr("Préparation de l’installation usine…"), async () => {
      try {
        const romPort = await selected;
        await closeFactorySession();
        $("factory-install-device").textContent = tr("Identification de la carte en cours…");
        if (port) await disconnect();
        expectedFirmware = undefined;
        factoryInstallRelease = undefined;
        factoryInstallSession = await prepareFactoryInstaller(romPort, undefined, undefined, reportFactoryInstallation);
        const release = factoryInstallRelease = factoryInstallSession.release;
        const device = factoryInstallSession.device;
        $("factory-install-release").textContent = tr("Image usine choisie : firmware {0} · {1}.{2}", release.firmwareVersion, device.label, release.hardwareValidated === false ? tr(" Premier démarrage à valider sur ce modèle.") : "");
        $("factory-install-device").textContent = `${device.chip} · Flash ${device.flashBytes / 1048576} Mio · PSRAM ${device.psramBytes / 1048576} Mio · MAC ${device.mac}`;
        reportFactoryInstallation(tr("Cette carte est prête. Vérifie son identité, puis confirme l’effacement ci-dessous. Aucune donnée n’a encore été effacée."));
      } catch (error) {
        reportFactoryInstallation(error.name === "NotFoundError" ? tr("Choix du port annulé. Aucune écriture effectuée.") : tr("Aucun effacement lancé : {0}", error.message));
        if (error.name !== "NotFoundError") throw error;
      }
    });
  } finally { factoryInstallWorking = false; renderFactoryInstallation(); }
};
const protectFactoryInstallation = event => {
  if (factoryInstallWriting) { event.preventDefault(); event.returnValue = ""; }
};
$("factory-install-start").onclick = async () => {
  const checked = $("factory-install-ack").checked, phrase = $("factory-install-phrase").value;
  if (busy || factoryInstallWorking || !factoryInstallSession || !factoryConsentValid(checked, phrase)) return;
  const session = factoryInstallSession, release = factoryInstallRelease;
  factoryInstallWorking = factoryInstallWriting = true;
  renderFactoryInstallation();
  $("factory-install-progress").hidden = false;
  $("factory-install-progress").value = 0;
  window.addEventListener("beforeunload", protectFactoryInstallation);
  try {
    await fileOperation(tr("Installation usine complète…"), async () => {
      try {
        const result = await session.install({ checked, phrase });
        expectedFirmware = { version: result.version, buildId: release.buildId };
        reportFactoryInstallation(tr("Image usine {0} écrite et vérifiée. Appuie sur RESET sans BOOT si nécessaire, puis reconnecte le Modulox dans l’éditeur pour vérifier son démarrage.", result.version), 100);
      } catch (error) {
        reportFactoryInstallation(error.factoryWriteStarted
          ? tr("Installation interrompue : {0} Remets le Modulox en BOOT et recommence l’installation complète.", error.message)
          : tr("Aucun effacement lancé : {0}", error.message));
        throw error;
      }
    });
  } finally {
    factoryInstallWorking = factoryInstallWriting = false;
    window.removeEventListener("beforeunload", protectFactoryInstallation);
    await closeFactorySession();
    renderFactoryInstallation();
  }
};
function verifyFirmwareRestart() {
  if (!expectedFirmware || !currentFirmware || currentFirmware.pendingVerify) return false;
  const matches = expectedFirmware.rollback
    ? currentFirmware.running !== expectedFirmware.fromSlot || currentFirmware.buildId !== expectedFirmware.fromBuild
    : currentFirmware.version === expectedFirmware.version && currentFirmware.buildId === expectedFirmware.buildId;
  if (!matches) {
    expectedFirmware = undefined;
    throw new Error(tr("Le Modulox a redémarré en version {0}. La nouvelle version n’a pas été confirmée ; un retour arrière a pu avoir lieu.", currentFirmware.version));
  }
  $("firmware-message").textContent = tr("Firmware {0} confirmé après redémarrage.", currentFirmware.version);
  expectedFirmware = undefined; return true;
}
async function reconnectAfterFirmware(previousPort) {
  const identity = previousPort.getInfo();
  await new Promise(resolve => setTimeout(resolve, 1800));
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    if (!port) {
      const approved = (await navigator.serial.getPorts()).filter(p => {
        const info = p.getInfo(); return p === previousPort || info.usbVendorId === identity.usbVendorId && info.usbProductId === identity.usbProductId;
      });
      const candidate = approved.includes(previousPort) ? previousPort : approved.length === 1 ? approved[0] : undefined;
      if (candidate) try { await candidate.open({ baudRate: 115200 }); await candidate.setSignals({ dataTerminalReady: true }); port = candidate; await readDevice(); }
      catch { if (port) await disconnect(); else await candidate.close().catch(() => {}); }
    }
    if (port) {
      if (!currentFirmware) throw new Error(tr("Le Modulox répond avec un ancien firmware. La nouvelle version n’est pas active ; reconnecte-le pour vérifier son état."));
      try { currentFirmware = await fetchFirmwareInfo(port); }
      catch { await disconnect(); }
      if (port && verifyFirmwareRestart()) return;
    }
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
  throw new Error(tr("Le transfert est terminé. Clique sur Connecter en USB pour retrouver le Modulox et vérifier son redémarrage."));
}
$("firmware-check").onclick = () => fileOperation(tr("Vérification du firmware…"), async () => {
  if (port && deviceCatalog?.firmware) { currentFirmware = await fetchFirmwareInfo(port); verifyFirmwareRestart(); }
  firmwareRelease = await fetchFirmwareRelease(fetch, currentFirmware?.model ?? defaultBoard.model); firmwareImage = undefined; firmwareManual = false;
  $("firmware-message").textContent = tr(firmwareRelease.notes ?? "Version disponible vérifiée.");
});
$("firmware-file-button").onclick = () => $("firmware-file").click();
$("firmware-file").onchange = () => {
  const file = $("firmware-file").files[0]; $("firmware-file").value = "";
  if (file) fileOperation(tr("Lecture de la mise à jour…"), async () => {
    const bundle = await readFirmwareFile(file);
    if (currentFirmware && bundle.release.model !== currentFirmware.model)
      throw new Error(tr("Ce fichier de firmware est destiné à un autre modèle de Modulox."));
    firmwareRelease = bundle.release; firmwareImage = bundle.image; firmwareManual = true;
    $("firmware-message").textContent = tr("Fichier {0} vérifié. Prêt pour une installation USB hors ligne.", bundle.release.version);
  });
};
$("firmware-cancel").onclick = () => firmwareAbort?.abort();
$("firmware").onclick = () => fileOperation(tr("Préparation du firmware…"), async () => {
  if (!port || !currentFirmware?.usbUpdate || !firmwareRelease) throw new Error(tr("Connecte un Modulox compatible avec la mise à jour A/B."));
  const release = structuredClone(firmwareRelease);
  const image = firmwareImage ?? await fetchFirmwareImage(release, currentFirmware);
  await validateFirmware(image, release, currentFirmware);
  if (!confirm(tr("Installer le firmware {0} à la place de {1} ?\nLe son sera interrompu et le Modulox redémarrera. Laisse le câble USB branché jusqu’à la fin.", release.version, currentFirmware.version))) return;
  const previousPort = port;
  firmwareAbort = new AbortController();
  $("firmware-progress").hidden = false; $("firmware-progress").value = 0; $("firmware-cancel").hidden = false;
  try {
    await uploadFirmware(port, image, release.sha256, (done, total) => {
      $("firmware-progress").value = Math.round(100 * done / total);
      $("firmware-message").textContent = `Transfert : ${Math.round(100 * done / total)} %`;
    }, firmwareAbort.signal);
    expectedFirmware = { version: release.version, buildId: release.buildId };
    $("firmware-cancel").hidden = true;
    $("firmware-message").textContent = tr("Transfert vérifié. Redémarrage et vérification du Modulox…");
    await disconnect(); await reconnectAfterFirmware(previousPort);
  } finally { firmwareAbort = undefined; $("firmware-cancel").hidden = true; }
});
$("firmware-rollback").onclick = () => fileOperation(tr("Retour à la version précédente…"), async () => {
  if (!port || !currentFirmware?.canRollback) return;
  if (!confirm(tr("Redémarrer sur le firmware précédent ? Le son sera interrompu. Les données de patches et samples ne sont pas effacées."))) return;
  const previousPort = port;
  const expected = { rollback: true, fromSlot: currentFirmware.running, fromBuild: currentFirmware.buildId };
  await rollbackFirmware(port); expectedFirmware = expected;
  $("firmware-message").textContent = tr("Redémarrage sur le firmware précédent…");
  await disconnect(); await reconnectAfterFirmware(previousPort);
});
$("firmware-recovery").onclick = () => fileOperation(tr("Récupération USB…"), async () => {
  if (!firmwareRelease) throw new Error(tr("Choisis d’abord un fichier de mise à jour ou vérifie les versions disponibles."));
  if (!confirm(tr("Récupération pour un Modulox déjà migré en A/B.\nPlace le Modulox en mode programmation avec BOOT maintenu pendant RESET, puis relâche BOOT et sélectionne le port USB de programmation.\nLe firmware sera écrit dans un emplacement inactif ; patches et samples ne seront pas effacés."))) return;
  if (port) await disconnect();
  const romPort = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a, usbProductId: 0x1001 }] });
  const release = structuredClone(firmwareRelease);
  const image = firmwareImage ?? await fetchFirmwareImage(release);
  $("firmware-progress").hidden = false; $("firmware-progress").value = 0;
  await recoverFirmware(romPort, image, release, (message, percent) => {
    $("firmware-message").textContent = message;
    if (percent !== undefined) $("firmware-progress").value = percent;
  });
  expectedFirmware = { version: release.version, buildId: release.buildId };
  $("firmware-message").textContent = tr("Récupération vérifiée. Clique sur Connecter en USB pour vérifier le démarrage et la version.");
});
fetchFirmwareRelease().then(release => { if (!currentFirmware || currentFirmware.model === release.model) firmwareRelease = release; renderFirmware(); }).catch(() => {
  $("firmware-latest").textContent = tr("Aucune version publiée sur cette copie. Un fichier de mise à jour peut être choisi hors ligne.");
});

window.addEventListener("resize", () => {
  if (screen === "editor" && editorView === "graph" && !drag) renderGraph();
});

render();
waveLibraryReady.then(() => {
  if (selectedNode) renderInspector();
});
if (!navigator.serial) showError(tr("Ouvre cette adresse dans Chrome ou Edge pour utiliser l’USB."));
Promise.all(["01-basic.json", "02-triple.json"].map(async (file) => {
  const response = await fetch("./examples/" + file);
  if (!response.ok) throw new Error(tr("Exemple indisponible."));
  return { file, patch: await response.json() };
})).then((items) => {
  examples = withKeys(items);
  if (!hasRead && !current()) select("example", examples[0].key);
  else renderLibrary();
}).catch(() => {});
