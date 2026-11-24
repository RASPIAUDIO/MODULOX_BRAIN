import assert from "node:assert/strict";
import vm from "node:vm";
import { readFile } from "node:fs/promises";
import { factoryConsentValid } from "../dist/factory-install.mjs";
const source = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const code = source.slice(source.indexOf("function renderFactoryInstallation()"), source.indexOf("function verifyFirmwareRestart()"));
assert.ok(code.includes('$("factory-install-start").onclick'));
const release = { firmwareVersion: "1.2.0", sha256: "a".repeat(64), buildId: "b".repeat(64) };
const elements = new Map(), writes = [], listeners = new Map();
const $ = id => {
  if (!elements.has(id)) elements.set(id, { value: "", checked: false, disabled: false, hidden: false, open: false, handlers: {},
    addEventListener(name, handler) { this.handlers[name] = handler; },
    showModal() { this.open = true; }, close() { this.open = false; this.handlers.close?.(); } });
  return elements.get(id);
};
let identity = 0, closed = 0, pauseInstall = false, finishWrite;
const context = { $, busy: "", port: undefined, expectedFirmware: undefined,
  factoryInstallGeneration: 0, factoryInstallWorking: false, factoryInstallWriting: false,
  factoryInstallRelease: undefined, factoryInstallImage: undefined, factoryInstallSession: undefined,
  factoryConsentValid, navigator: { serial: { requestPort: async () => ({}) } },
  window: { addEventListener(name, fn) { listeners.set(name, fn); }, removeEventListener(name) { listeners.delete(name); } },
  fetchFactoryRelease: async () => release, fetchFactoryImage: async () => new Uint8Array(1),
  prepareFactoryInstaller: async (_port, image, chosenRelease) => {
    assert.equal(image, undefined); assert.equal(chosenRelease, undefined); // Identify before choosing either model's image.
    return { release, device: { chip: "ESP32-S3", flashBytes: 0x1000000, psramBytes: 0x800000, label: "Modulox 16 / 8", mac: `74:4d:bd:00:00:0${++identity}` },
    close: async () => { ++closed; }, install: async consent => {
      writes.push({ identity, ...consent });
      if (pauseInstall) await new Promise(resolve => { finishWrite = resolve; });
      return { version: "1.2.0" };
    } }; },
  fileOperation: async (_label, run) => run(), disconnect: async () => {},
};
vm.runInNewContext(code, context);
await $("firmware-factory").onclick();
assert.equal($("factory-install-start").disabled, true);
$("factory-install-ack").checked = true; $("factory-install-phrase").value = "EFFACER";
$("factory-install-phrase").oninput();
assert.equal($("factory-install-start").disabled, true); // Text alone cannot authorize an unidentified board.
await $("factory-install-start").onclick(); assert.equal(writes.length, 0);
await $("factory-install-prepare").onclick();
assert.equal($("factory-install-ack").checked, false); assert.equal($("factory-install-phrase").value, "");
assert.match($("factory-install-device").textContent, /00:00:01/);
$("factory-install-ack").checked = true; $("factory-install-phrase").value = "EFFACER";
$("factory-install-phrase").oninput(); assert.equal($("factory-install-start").disabled, false);
await $("factory-install-start").onclick(); assert.equal(writes.length, 1);
assert.equal($("factory-install-start").disabled, true); assert.equal($("factory-install-phrase").value, "");
assert.equal(listeners.has("beforeunload"), false);
await $("factory-install-prepare").onclick();
assert.match($("factory-install-device").textContent, /00:00:02/);
assert.equal($("factory-install-start").disabled, true); assert.equal($("factory-install-ack").checked, false);
await $("factory-install-start").onclick(); assert.equal(writes.length, 1);
$("factory-install-close").onclick(); await Promise.resolve();
assert.equal($("factory-install-start").disabled, true); assert.equal(closed, 2);
await $("firmware-factory").onclick(); await $("factory-install-prepare").onclick();
$("factory-install-ack").checked = true; $("factory-install-phrase").value = "EFFACER";
pauseInstall = true;
const installing = $("factory-install-start").onclick();
let leavingPrevented = false, dialogPrevented = false;
listeners.get("beforeunload")({ preventDefault() { leavingPrevented = true; }, returnValue: undefined });
$("factory-install-dialog").handlers.cancel({ preventDefault() { dialogPrevented = true; } });
$("factory-install-close").onclick();
assert.equal(leavingPrevented, true); assert.equal(dialogPrevented, true);
assert.equal($("factory-install-dialog").open, true); assert.equal($("factory-install-close").disabled, true);
finishWrite(); await installing;
assert.equal(listeners.has("beforeunload"), false); assert.equal($("factory-install-close").disabled, false);
console.log("PASS: UI requires an identified board plus checkbox and exact phrase; consent clears on device change, completion and close; no automatic second-unit flash");
