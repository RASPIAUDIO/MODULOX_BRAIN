import { tr } from "./i18n.mjs";
import { boardForModel, boardAssetPath, defaultBoard } from "./hardware.mjs";
export const firmwareModel = defaultBoard.model;
export const firmwareLayout = defaultBoard.layout;
export const firmwareSlotBytes = defaultBoard.slotBytes;
const text = (bytes, start, length) => new TextDecoder().decode(bytes.subarray(start, start + length)).split("\0")[0];
export function compareVersions(a, b) {
  const parse = value => {
    const m = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?$/.exec(value);
    if (!m) throw new Error(tr("Version de firmware invalide."));
    return { numbers: m.slice(1, 4).map(Number), pre: m[4]?.split(".") };
  };
  const x = parse(a), y = parse(b);
  for (let i = 0; i < 3; i++) if (x.numbers[i] !== y.numbers[i]) return Math.sign(x.numbers[i] - y.numbers[i]);
  if (!x.pre || !y.pre) return x.pre ? -1 : y.pre ? 1 : 0;
  for (let i = 0; i < Math.max(x.pre.length, y.pre.length); i++) {
    if (x.pre[i] === undefined || y.pre[i] === undefined) return x.pre[i] === undefined ? -1 : 1;
    if (x.pre[i] === y.pre[i]) continue;
    const xn = /^\d+$/.test(x.pre[i]), yn = /^\d+$/.test(y.pre[i]);
    return xn && yn ? Math.sign(Number(x.pre[i]) - Number(y.pre[i])) : xn ? -1 : yn ? 1 : x.pre[i] < y.pre[i] ? -1 : 1;
  }
  return 0;
}
export function validateRelease(release) {
  const board = boardForModel(release?.model);
  if (!release || release.chip !== "esp32s3" || release.layout !== board.layout || release.protocol !== 1 ||
      !Number.isInteger(release.size) || release.size < 288 || release.size > firmwareSlotBytes || !/^[a-f0-9]{64}$/.test(release.sha256) ||
      !/^[a-f0-9]{64}$/.test(release.buildId ?? "")) throw new Error(tr("Mise à jour incompatible ou manifeste invalide."));
  compareVersions(release.version, release.version);
  return release;
}
export async function validateFirmware(image, release, device) {
  validateRelease(release);
  if (!(image instanceof Uint8Array) || image.length !== release.size || image[0] !== 0xe9) throw new Error(tr("Taille ou format du firmware invalide."));
  const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
  if (view.getUint16(12, true) !== 9 || view.getUint32(32, true) !== 0xabcd5432 ||
      text(image, 80, 32) !== boardForModel(release.model).project || text(image, 48, 32) !== release.version) throw new Error(tr("Image destinée à un autre appareil ou version incohérente."));
  const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, "0")).join("");
  if (hex(image.subarray(176, 208)) !== release.buildId) throw new Error(tr("Identifiant de compilation incohérent."));
  if (hex(new Uint8Array(await crypto.subtle.digest("SHA-256", image))) !== release.sha256) throw new Error(tr("Le contrôle SHA-256 du firmware a échoué."));
  if (device && (device.model !== release.model || device.layout !== release.layout || !device.usbUpdate || device.protocol !== 1 || image.length > device.slotBytes)) throw new Error(tr("Le Modulox connecté ne peut pas recevoir cette mise à jour. Une migration initiale peut être nécessaire."));
  return image;
}
export async function readFirmwareFile(file) {
  if (file.size > 3 * 1024 * 1024) throw new Error(tr("Fichier de firmware trop volumineux."));
  let bundle;
  try { bundle = JSON.parse(await file.text()); } catch { throw new Error(tr("Choisis un fichier .modulox-fw.json valide.")); }
  if (bundle?.format !== "modulox.firmware" || bundle.version !== 1 || typeof bundle.image !== "string" || !/^[A-Za-z0-9+/]*={0,2}$/.test(bundle.image)) throw new Error(tr("Format de mise à jour incompatible."));
  const release = validateRelease(bundle.release);
  const image = Uint8Array.from(atob(bundle.image), c => c.charCodeAt(0));
  await validateFirmware(image, release);
  return { release, image };
}
export async function fetchFirmwareRelease(request = fetch, model = defaultBoard.model) {
  const response = await request(boardAssetPath("firmware", boardForModel(model)) + "latest.json", { cache: "no-store" });
  if (!response.ok) throw new Error(tr("Aucune version de firmware n’est publiée sur cette copie de l’éditeur. Tu peux choisir un fichier de mise à jour."));
  const release = validateRelease(await response.json());
  if (release.model !== model) throw new Error(tr("Le serveur a fourni une version destinée à un autre modèle."));
  return release;
}
export async function fetchFirmwareImage(release, device, request = fetch) {
  validateRelease(release);
  if (!/^[a-z0-9.-]+\.bin$/.test(release.file ?? "")) throw new Error(tr("Chemin de firmware invalide."));
  const response = await request(boardAssetPath("firmware", boardForModel(release.model)) + release.file, { cache: "no-store" });
  if (!response.ok) throw new Error(tr("Le fichier de firmware est indisponible."));
  const image = new Uint8Array(await response.arrayBuffer());
  await validateFirmware(image, release, device); return image;
}
