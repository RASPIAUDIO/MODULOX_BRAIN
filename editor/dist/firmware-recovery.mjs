import { tr } from "./i18n.mjs";
import { validateFirmware } from "./firmware.mjs";
import { defaultBoard, identifyBoard } from "./hardware.mjs";

// CRC convention used by ESP-IDF's otatool: binascii.crc32(seq, 0xffffffff).
export function otaCrc(bytes) {
  let crc = 0;
  for (const byte of bytes) { crc ^= byte; for (let i = 0; i < 8; i++) crc = crc & 1 ? 0xedb88320 ^ crc >>> 1 : crc >>> 1; }
  return (crc ^ 0xffffffff) >>> 0;
}
export function recoveryPlan(table, metadata, board = defaultBoard) {
  const entries = new Map(), view = new DataView(table.buffer, table.byteOffset, table.byteLength);
  for (let offset = 0; offset + 32 <= table.length && view.getUint16(offset, true) === 0x50aa; offset += 32) {
    const name = new TextDecoder().decode(table.subarray(offset + 12, offset + 28)).split("\0")[0];
    if (entries.has(name)) throw new Error(tr("Table des partitions invalide."));
    entries.set(name, { type: view.getUint8(offset + 2), subtype: view.getUint8(offset + 3), address: view.getUint32(offset + 4, true), size: view.getUint32(offset + 8, true) });
  }
  for (const [name, address, size] of [["nvs", 0x9000, 0x6000], ["phy_init", 0xf000, 0x1000], ["factory", 0x10000, 0x100000], ["otadata", 0x1f0000, 0x2000],
    ["ota_0", board.ota0, board.slotBytes], ["ota_1", board.ota1, board.slotBytes], ["patches", 0x110000, 0x20000],
    ["userpatches", 0x130000, 0x20000], ["waves", 0x150000, 0xa0000], ["samples", 0x200000, board.sampleBytes]]) {
    const entry = entries.get(name);
    if (!entry || entry.address !== address || entry.size !== size) throw new Error(tr("Cette carte nécessite la migration initiale A/B avant la récupération depuis Chrome."));
  }
  if (entries.get("ota_0").subtype !== 0x10 || entries.get("ota_1").subtype !== 0x11 || entries.get("otadata").type !== 1 || entries.get("otadata").subtype !== 0) throw new Error(tr("Types de partitions incompatibles."));
  if (metadata.length !== 8192) throw new Error(tr("Métadonnées OTA incomplètes."));
  const data = new DataView(metadata.buffer, metadata.byteOffset, metadata.byteLength), records = [];
  for (let i = 0; i < 2; i++) {
    const offset = i * 4096, seq = data.getUint32(offset, true), state = data.getUint32(offset + 24, true);
    if (seq !== 0xffffffff && seq > 0 && data.getUint32(offset + 28, true) === otaCrc(metadata.subarray(offset, offset + 4))) records.push({ index: i, seq, state, slot: (seq - 1) % 2 });
  }
  // Preserve the most recent CONFIRMED slot; a failed first boot must not become the backup.
  const valid = records.filter(r => r.state === 2 || r.state === 0xffffffff).sort((a, b) => b.seq - a.seq)[0];
  const slot = valid ? 1 - valid.slot : 0;
  let seq = Math.max(0, ...records.map(r => r.seq)) + 1;
  while ((seq - 1) % 2 !== slot) seq++;
  if (seq >= 0xffffffff) throw new Error(tr("Compteur OTA épuisé ; intervention technique nécessaire."));
  const page = new Uint8Array(4096).fill(255), pageView = new DataView(page.buffer);
  pageView.setUint32(0, seq, true); pageView.setUint32(24, 0, true); pageView.setUint32(28, otaCrc(page.subarray(0, 4)), true);
  return { address: slot === 0 ? board.ota0 : board.ota1, metadataAddress: 0x1f0000 + (valid ? 1 - valid.index : 0) * 4096, metadata: page };
}
export async function recoverFirmware(romPort, image, release, report = () => {}) {
  await validateFirmware(image, release);
  const { ESPLoader, Transport } = await import("./vendor/esptool/index.js");
  const transport = new Transport(romPort);
  const terminal = { clean() {}, writeLine() {}, write() {} };
  const loader = new ESPLoader({ transport, baudrate: 460800, romBaudrate: 115200, terminal, debugLogging: false });
  try {
    report(tr("Connexion au mode de récupération…"));
    await loader.main("no_reset");
    if (loader.chip.CHIP_NAME !== "ESP32-S3") throw new Error(tr("Le périphérique sélectionné n’est pas un ESP32-S3."));
    const board = await identifyBoard(loader);
    if (board.model !== release.model) throw new Error(tr("Ce firmware est destiné à un autre modèle de Modulox."));
    const table = await loader.readFlash(0x8000, 4096), metadata = await loader.readFlash(0x1f0000, 8192);
    const plan = recoveryPlan(table, metadata, board);
    const write = async (address, bytes, progress) => loader.writeFlash({ fileArray: [{ address, data: bytes }],
      flashSize: "keep", flashMode: "keep", flashFreq: "keep", eraseAll: false, compress: true,
      reportProgress: (index, done, total) => progress?.(Math.round(100 * done / total)) });
    report(tr("Écriture dans l’emplacement inactif…"), 0);
    await write(plan.address, image, percent => report(tr("Récupération du firmware"), percent));
    report(tr("Relecture et vérification du firmware…"));
    const written = await loader.readFlash(plan.address, image.length);
    await validateFirmware(written, release);
    // Only the unused metadata sector is erased. The last confirmed boot record survives interruption.
    await write(plan.metadataAddress, plan.metadata);
    const checked = await loader.readFlash(plan.metadataAddress, 4096);
    if (!checked.every((byte, i) => byte === plan.metadata[i])) throw new Error(tr("La sélection du firmware n’a pas pu être vérifiée."));
    report(tr("Récupération vérifiée. Redémarrage…"), 100);
    await loader.after("hard_reset");
  } finally { await transport.disconnect().catch(() => {}); }
}
