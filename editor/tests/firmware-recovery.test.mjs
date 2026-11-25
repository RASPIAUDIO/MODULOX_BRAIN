import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import vm from "node:vm";
import { otaCrc, recoveryPlan } from "../dist/firmware-recovery.mjs";
const root = new URL("../../firmware/", import.meta.url);
const table = new Uint8Array(await readFile(new URL("build/partition_table/partition-table.bin", root)));
const fresh = new Uint8Array(8192).fill(255);
let plan = recoveryPlan(table, fresh);
assert.equal(plan.address, 0xc00000); assert.equal(plan.metadataAddress, 0x1f0000);
assert.equal(new DataView(plan.metadata.buffer).getUint32(24, true), 0);
assert.equal(new DataView(plan.metadata.buffer).getUint32(28, true), otaCrc(plan.metadata.subarray(0, 4)));
fresh.set(plan.metadata);
new DataView(fresh.buffer).setUint32(24, 2, true); // A confirmed.
plan = recoveryPlan(table, fresh);
assert.equal(plan.address, 0xd80000); assert.equal(plan.metadataAddress, 0x1f1000);
fresh.set(plan.metadata, 4096); new DataView(fresh.buffer).setUint32(4096 + 24, 1, true); // B pending and broken.
plan = recoveryPlan(table, fresh);
assert.equal(plan.address, 0xd80000); assert.equal(plan.metadataAddress, 0x1f1000); // Preserve confirmed A.
const invalid = table.slice(); new DataView(invalid.buffer).setUint32(4, 0x1234, true);
await assert.rejects(async () => recoveryPlan(invalid, fresh));
const generated = new Uint8Array(await readFile(new URL("releases/1.0.0/boot-a.bin", root)));
assert.equal(new DataView(generated.buffer).getUint32(28, true), otaCrc(generated.subarray(0, 4)));
const library = await import("../dist/vendor/esptool/index.js");
assert.equal(typeof library.ESPLoader, "function");
assert.equal(typeof library.Transport, "function");
const { getStubJsonByChipName } = library;
const stub = await getStubJsonByChipName("ESP32-S3", 2); assert.ok(stub.decodedText.length > 100);
console.log("PASS: ROM recovery preserves confirmed slot, writes only inactive app and alternate metadata sector, CRC matches ESP-IDF, offline esptool/stub imports work");

const app = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const start = app.indexOf('$("firmware-recovery").onclick =');
const end = app.indexOf("\nfetchFirmwareRelease().then", start);
assert.ok(start >= 0 && end > start);
const elements = new Map();
const $ = id => {
  if (!elements.has(id)) elements.set(id, { open: false, showModal() { this.open = true; }, close() { this.open = false; } });
  return elements.get(id);
};
const events = [];
const rom = {};
const image = Uint8Array.of(1);
const release = { version: "1.7.0-dev.1", buildId: "release" };
let choosePort = false;
const context = {
  $, busy: "", firmwareRelease: release, firmwareImage: image, port: {}, expectedFirmware: undefined,
  structuredClone, tr: text => text,
  navigator: { serial: { async requestPort({ filters: [filter] }) {
    assert.equal(filter.usbVendorId, 0x303a); assert.equal(filter.usbProductId, 0x1001);
    events.push("select");
    if (!choosePort) throw new DOMException("Cancelled", "NotFoundError");
    return rom;
  } } },
  fileOperation: async (_label, operation) => { events.push("operation"); return operation(); },
  disconnect: async () => events.push("disconnect"),
  recoverFirmware: async (selected, data, update) => {
    assert.equal(selected, rom); assert.equal(data, image); assert.equal(update.version, release.version);
    events.push("recover");
  },
};
vm.runInNewContext(app.slice(start, end), context);
$("firmware-recovery").onclick();
assert.ok($("firmware-recovery-dialog").open);
assert.deepEqual(events, []);
await $("firmware-recovery-port").onclick();
assert.ok($("firmware-recovery-dialog").open);
assert.deepEqual(events, ["select", "operation"]);
choosePort = true;
await $("firmware-recovery-port").onclick();
assert.ok(!$("firmware-recovery-dialog").open);
assert.deepEqual(events.slice(2), ["select", "operation", "disconnect", "recover"]);
assert.equal(context.expectedFirmware.buildId, release.buildId);
