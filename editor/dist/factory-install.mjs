import { compareVersions } from "./firmware.mjs";
import { otaCrc, recoveryPlan } from "./firmware-recovery.mjs";
import { defaultBoard, boardForModel, boardAssetPath, identifyBoard } from "./hardware.mjs";

export const factoryFlashBytes = defaultBoard.flashBytes;
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, "0")).join("");
const text = (image, start, count) => new TextDecoder().decode(image.subarray(start, start + count)).split("\0")[0];

export function validateFactoryRelease(release) {
  const board = boardForModel(release?.model);
  if (release?.format !== "modulox.full-flash" || release.version !== 1 ||
      release.chip !== "esp32s3" || release.layout !== board.layout || release.offset !== 0 ||
      release.size !== board.flashBytes || release.flashBytes !== board.flashBytes || release.psramBytes !== board.psramBytes ||
      !/^[a-f0-9]{64}$/.test(release.sha256 ?? "") || !/^[a-f0-9]{32}$/.test(release.md5 ?? "") ||
      !/^[a-f0-9]{64}$/.test(release.buildId ?? "") || !["ota_0", "ota_1"].includes(release.bootSlot) ||
      !/^[a-z0-9.-]+\.bin$/.test(release.file ?? "")) throw new Error("Image usine incompatible ou manifeste invalide.");
  compareVersions(release.firmwareVersion, release.firmwareVersion);
  return release;
}

export async function validateFactoryImage(image, release) {
  validateFactoryRelease(release);
  const board = boardForModel(release.model);
  if (!(image instanceof Uint8Array) || image.length !== board.flashBytes || image[0] !== 0xe9)
    throw new Error(`L’image usine doit contenir les ${board.flashBytes / 1048576} Mio complets de la flash.`);
  if (hex(new Uint8Array(await crypto.subtle.digest("SHA-256", image))) !== release.sha256)
    throw new Error("Le contrôle SHA-256 de l’image usine a échoué.");
  const data = new DataView(image.buffer, image.byteOffset, image.byteLength);
  if (data.getUint16(12, true) !== 9) throw new Error("Bootloader incompatible avec le Modulox ESP32-S3.");
  const metadata = image.subarray(0x1f0000, 0x1f2000);
  recoveryPlan(image.subarray(0x8000, 0x9000), metadata, board); // Validate the complete expected partition layout.
  const records = [];
  for (let sector = 0; sector < 2; ++sector) {
    const offset = 0x1f0000 + sector * 4096;
    const sequence = data.getUint32(offset, true), state = data.getUint32(offset + 24, true);
    if (sequence > 0 && sequence !== 0xffffffff && [0, 2, 0xffffffff].includes(state) &&
        data.getUint32(offset + 28, true) === otaCrc(image.subarray(offset, offset + 4)))
      records.push({ sequence, slot: "ota_" + (sequence - 1) % 2 });
  }
  const selected = records.sort((a, b) => b.sequence - a.sequence)[0];
  const offset = release.bootSlot === "ota_0" ? board.ota0 : board.ota1;
  if (selected?.slot !== release.bootSlot || image[offset] !== 0xe9 || data.getUint16(offset + 12, true) !== 9 ||
      data.getUint32(offset + 32, true) !== 0xabcd5432 || text(image, offset + 80, 32) !== board.project ||
      text(image, offset + 48, 32) !== release.firmwareVersion || hex(image.subarray(offset + 176, offset + 208)) !== release.buildId)
    throw new Error("La sélection de démarrage de l’image usine est incohérente.");
  return image;
}

export async function fetchFactoryRelease(request = fetch, model = defaultBoard.model) {
  const response = await request(boardAssetPath("factory", boardForModel(model)) + "latest.json", { cache: "no-store" });
  if (!response.ok) throw new Error("Aucune image usine complète n’est fournie avec cette copie de l’éditeur.");
  const release = validateFactoryRelease(await response.json());
  if (release.model !== model) throw new Error("L’image usine fournie ne correspond pas au modèle détecté.");
  return release;
}
export async function fetchFactoryImage(release, request = fetch) {
  validateFactoryRelease(release);
  const response = await request(boardAssetPath("factory", boardForModel(release.model)) + release.file, { cache: "no-store" });
  if (!response.ok) throw new Error("L’image usine complète est indisponible.");
  const image = new Uint8Array(await response.arrayBuffer());
  await validateFactoryImage(image, release);
  return image;
}
export const factoryConsentValid = (checked, phrase) => checked === true && typeof phrase === "string" && phrase.trim() === "EFFACER";

async function defaultConnection(port) {
  const { ESPLoader, Transport } = await import("./vendor/esptool/index.js");
  const transport = new Transport(port);
  const loader = new ESPLoader({ transport, baudrate: 460800, romBaudrate: 115200,
    terminal: { clean() {}, writeLine() {}, write() {} }, debugLogging: false });
  return { loader, transport };
}

// Preparing a session never writes flash. The destructive action is a separate,
// one-use method, guarded by fresh consent for this identified target.
export async function prepareFactoryInstaller(port, image, release, report = () => {}, connect = defaultConnection) {
  if (image || release) await validateFactoryImage(image, release);
  const { loader, transport } = await connect(port);
  let closed = false, used = false;
  const close = async () => { if (!closed) { closed = true; await transport.disconnect(); } };
  try {
    report("Identification du Modulox en mode BOOT…", 0);
    await loader.main("no_reset");
    if (loader.chip.CHIP_NAME !== "ESP32-S3") throw new Error("Cette installation nécessite un ESP32-S3.");
    const security = await loader.getSecurityInfo(false);
    if (security?.chipId !== 9 || !Number.isInteger(security.flags) || (security.flags & 5) !== 0 || security.flashCryptCnt !== 0)
      throw new Error("Carte protégée ou sécurité non vérifiable : l’installation usine complète est refusée.");
    const board = await identifyBoard(loader);
    if (release && release.model !== board.model) throw new Error("Cette image usine est destinée à un autre modèle de Modulox.");
    const mac = await loader.chip.readMac(loader);
    if (!/^(?:[a-f0-9]{2}:){5}[a-f0-9]{2}$/i.test(mac)) throw new Error("Impossible d’identifier précisément cette carte.");
    const device = Object.freeze({ chip: "ESP32-S3", model: board.model, label: board.label,
      flashBytes: board.flashBytes, psramBytes: board.psramBytes, mac });
    if (!release) {
      report(`${board.label} identifié. Chargement de son image usine…`, 0);
      release = await fetchFactoryRelease(fetch, board.model);
      image = await fetchFactoryImage(release);
    }
    report("Carte identifiée. Aucune donnée n’a encore été effacée.", 0);
    return {
      device, release, close,
      async install({ checked, phrase }) {
        if (closed || used) throw new Error("Prépare de nouveau cette carte avant de l’installer.");
        if (!factoryConsentValid(checked, phrase)) throw new Error("Coche l’avertissement et saisis EFFACER avant l’installation.");
        used = true;
        let writeStarted = false;
        try {
          await validateFactoryImage(image, release);
          report("Écriture de l’image usine complète. Ne débranche pas le Modulox…", 1);
          writeStarted = true;
          await loader.writeFlash({ fileArray: [{ address: 0, data: image }], flashSize: "keep", flashMode: "keep", flashFreq: "keep",
            eraseAll: false, compress: true, reportProgress: (_index, done, total) => {
              const percent = Math.round(100 * done / total);
              report(`Installation usine : ${percent} %`, 1 + Math.round(93 * done / total));
            } });
          report(`Vérification des ${board.flashBytes / 1048576} Mio écrits…`, 95);
          if (String(await loader.flashMd5sum(0, board.flashBytes)).toLowerCase() !== release.md5)
            throw new Error("La vérification de la flash complète a échoué.");
          report("Image usine écrite et vérifiée.", 100);
          try { await loader.after("hard_reset"); } catch { /* USB may disappear on reset; the image was verified. */ }
          return { version: release.firmwareVersion, mac };
        } catch (error) {
          error.factoryWriteStarted = writeStarted;
          throw error;
        } finally { await close().catch(() => {}); }
      },
    };
  } catch (error) { await close().catch(() => {}); throw error; }
}
