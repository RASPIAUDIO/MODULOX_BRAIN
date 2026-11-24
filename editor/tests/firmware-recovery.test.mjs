import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
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
