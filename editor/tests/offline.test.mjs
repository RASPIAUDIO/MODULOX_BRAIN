import assert from "node:assert/strict";
import { readFile, readdir, mkdtemp, rm } from "node:fs/promises";
import { spawn } from "node:child_process";
import { request as httpRequest } from "node:http";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { validatePatch, offlineCatalog } from "../dist/patch.mjs";
import { loadPresetAssets } from "../dist/preset-assets.mjs";

const editor = fileURLToPath(new URL("../", import.meta.url));
const powershell = join(process.env.SystemRoot, "System32/WindowsPowerShell/v1.0/powershell.exe");
const releases = join(editor, "releases");
const temp = await mkdtemp(join(releases, "test with spaces-"));
const port = 8767;
function run(args) {
  return new Promise((resolve, reject) => {
    const child = spawn(powershell, ["-NoProfile", "-ExecutionPolicy", "Bypass", ...args], { windowsHide: true });
    let text = ""; child.stdout.on("data", value => text += value); child.stderr.on("data", value => text += value);
    child.on("error", reject); child.on("exit", code => code === 0 ? resolve(text) : reject(new Error(text)));
  });
}
function request(path, options = {}) {
  return new Promise((resolve, reject) => {
    const req = httpRequest({ host: "127.0.0.1", port, path: path.startsWith("./") ? path.slice(1) : path, agent: false, method: options.method ?? "GET", headers: options.headers ?? {}, timeout: 5000 }, response => {
      const parts = []; response.on("data", part => parts.push(part));
      response.on("end", () => { const bytes = Buffer.concat(parts); resolve({ status: response.statusCode, headers: response.headers, ok: response.statusCode === 200,
        json: async () => JSON.parse(bytes), arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength), bytes }); });
      response.on("error", reject);
    });
    req.on("error", reject); req.on("timeout", () => req.destroy(new Error("HTTP timeout"))); req.end();
  });
}
const quote = text => "'" + text.replaceAll("'", "''") + "'";
let script;
try {
  await run(["-Command", `Expand-Archive -LiteralPath ${quote(join(releases, "Modulox-Windows-offline.zip"))} -DestinationPath ${quote(temp)}`]);
  const packageRoot = join(temp, "Modulox"); script = join(packageRoot, "offline.ps1");
  const names = await readdir(packageRoot);
  assert.ok(names.includes("lancer.cmd") && names.includes("arreter.cmd"));
  assert.ok(!names.some(name => /\.(exe|dll)$/i.test(name)));
  await run(["-File", script, "-Port", String(port), "-NoBrowser"]);
  const first = await (await request("/__modulox/status")).json(); assert.equal(first.app, "modulox-offline");
  // Relaunch reuses the same worker rather than binding another listener.
  await run(["-File", script, "-Port", String(port), "-NoBrowser"]);
  assert.equal((await (await request("/__modulox/status")).json()).instance, first.instance);
  const root = await request("/"); assert.equal(root.status, 200); assert.match(root.bytes.toString(), /Mes patches/);
  assert.match((await request("/i18n.mjs")).headers["content-type"], /javascript/);
  assert.match((await request("/midi-keyboard.mjs")).headers["content-type"], /javascript/);
  assert.match((await request("/device-screen.mjs")).headers["content-type"], /javascript/);
  assert.match((await request("/flags/jp.png")).headers["content-type"], /image\/png/);
  assert.equal((await (await request("/locales/ja.json")).json()).Langue, "言語");
  for (const path of ["/app.mjs?v=16", "/patch.mjs", "/resources.mjs", "/patch-files.mjs", "/preset-assets.mjs", "/firmware.mjs", "/firmware-recovery.mjs", "/firmware/latest.json", "/vendor/esptool/index.js", "/vendor/esptool/targets/stub_flasher/esp32s3.json", "/style.css", "/examples/01-basic.json", "/examples/02-triple.json", "/demo-samples/voice.wav"]) {
    const response = await request(path); assert.equal(response.status, 200, path);
    if (path.includes(".mjs")) assert.match(response.headers["content-type"], /javascript/);
  }
  const head = await request("/app.mjs", { method: "HEAD" }); assert.equal(head.bytes.length, 0); assert.ok(Number(head.headers["content-length"]) > 0);
  const factoryModule = await request("/factory-bank.mjs"); assert.equal(factoryModule.status, 200); assert.match(factoryModule.headers["content-type"], /javascript/);
  assert.equal((await request("/factory-install.mjs")).status, 200);
  const factory = await (await request("/factory/latest.json")).json();
  assert.equal(factory.size, 0x1000000);
  assert.equal((await request("/factory/" + factory.file)).bytes.length, factory.size);
  for (const path of ["/hardware.mjs", "/board-profiles.mjs"]) assert.equal((await request(path)).status, 200);
  const largeFactory = await (await request("/factory/s3-32m16m/latest.json")).json();
  assert.equal(largeFactory.size, 0x2000000);
  assert.equal((await request("/factory/s3-32m16m/" + largeFactory.file)).bytes.length, largeFactory.size);
  const largeRelease = await (await request("/firmware/s3-32m16m/latest.json")).json();
  assert.equal((await request("/firmware/s3-32m16m/" + largeRelease.file)).bytes.length, largeRelease.size);
  const files = await (await request("/presets/index.json")).json(); assert.equal(files.length, 40);
  const waves = new Map(), samples = new Map();
  const libraries = { waves, samples, fetch: request, saveWaves: async () => {}, saveSample: async () => {} };
  for (const file of files) {
    const patch = await (await request("/presets/" + file)).json(); assert.equal(validatePatch(patch, offlineCatalog), "", file);
    assert.equal(patch.settings.Volume, 85, file);
    await loadPresetAssets(patch, libraries);
  }
  assert.equal(waves.size, 4); assert.equal(samples.size, 3);
  const release = await (await request("/firmware/latest.json")).json();
  const binary = await request("/firmware/" + release.file);
  assert.equal(binary.bytes.length, release.size);
  await Promise.all(Array.from({ length: 20 }, async () => assert.equal((await request("/app.mjs")).status, 200)));
  for (const path of ["/../offline.ps1", "/%2e%2e/offline.ps1", "/presets/assets/../../offline.ps1", "/HORS-LIGNE.txt", "/presets/personal.modulox.json"])
    assert.equal((await request(path)).status, 404, path);
  assert.equal((await request("/", { headers: { Host: "outside.example" } })).status, 403);
  assert.equal((await request("/presets/personal.json", { method: "PUT" })).status, 405);
  assert.equal((await request("/__modulox/stop", { method: "POST" })).status, 403);
  assert.equal((await request("/__modulox/status")).status, 200);
  await run(["-File", script, "-Port", String(port), "-Stop"]);
  await assert.rejects(request("/__modulox/status"));
  // Windows socket metadata must expose only the loopback address while running.
  console.log("PASS: extracted offline ZIP in a path with spaces, Windows PowerShell launch/relaunch/stop, 40 presets + all PCM, 20 concurrent requests, HEAD/MIME, no file uploads/traversal/external hosts");
} finally {
  if (script) await run(["-File", script, "-Port", String(port), "-Stop"]).catch(() => {});
  const target = resolve(temp), parent = resolve(releases) + "/";
  assert.ok(target.replaceAll("\\", "/").startsWith(parent.replaceAll("\\", "/")));
  await rm(target, { recursive: true, force: true });
}
