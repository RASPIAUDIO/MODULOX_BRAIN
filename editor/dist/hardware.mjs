import { boardProfiles } from "./board-profiles.mjs";
export { boardProfiles };
export const defaultBoard = boardProfiles["s3-16m8m"];
export function boardForModel(model) {
  const board = Object.values(boardProfiles).find(board => board.model === model);
  if (!board) throw new Error("Modèle de Modulox inconnu : aucune image compatible ne sera sélectionnée.");
  return board;
}
export const boardAssetPath = (kind, board) => `/${kind}/` + (board.id === defaultBoard.id ? "" : board.id + "/");

// Read-only identification in ROM mode. Capacities are eFuse package fields,
// not the firmware currently installed or the USB product name.
export async function identifyBoard(loader) {
  if (loader.chip.CHIP_NAME !== "ESP32-S3") throw new Error("Cette installation nécessite un ESP32-S3.");
  const flash = await loader.detectFlashSize();
  const word4 = await loader.readReg(0x60007054), word5 = await loader.readReg(0x60007058);
  const flashWord = await loader.readReg(0x6000703c);
  if (![word4, word5, flashWord].every(Number.isInteger)) throw new Error("Identification mémoire incomplète.");
  const cap = ((word4 >>> 3) & 3) | (((word5 >>> 19) & 1) << 2);
  const psramBytes = ({ 1: 8, 2: 2, 3: 16, 4: 4 }[cap] ?? 0) * 1024 * 1024;
  const psramVendor = (word4 >>> 7) & 3, octalFlash = Boolean((flashWord >>> 9) & 1);
  const board = Object.values(boardProfiles).find(board => flash === `${board.flashBytes / 1048576}MB` &&
    board.psramBytes === psramBytes && board.psramVendor === psramVendor && board.octalFlash === octalFlash);
  if (!board) throw new Error("Configuration mémoire inconnue : installation refusée pour protéger cette carte.");
  return board;
}
