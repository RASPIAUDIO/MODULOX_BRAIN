import { createServer } from "node:http";
import { readFile, readdir } from "node:fs/promises";

const assets = new Map([
  ["/", ["./dist/index.html", "text/html"]],
  ["/style.css", ["./dist/style.css", "text/css"]],
  ["/app.mjs", ["./dist/app.mjs", "text/javascript"]],
  ["/protocol.mjs", ["./dist/protocol.mjs", "text/javascript"]],
  ["/patch.mjs", ["./dist/patch.mjs", "text/javascript"]],
  ["/waves.mjs", ["./dist/waves.mjs", "text/javascript"]],
  ["/samples.mjs", ["./dist/samples.mjs", "text/javascript"]],
  ["/preset-assets.mjs", ["./dist/preset-assets.mjs", "text/javascript"]],
  ["/factory-bank.mjs", ["./dist/factory-bank.mjs", "text/javascript"]],
  ["/factory-install.mjs", ["./dist/factory-install.mjs", "text/javascript"]],
  ["/patch-files.mjs", ["./dist/patch-files.mjs", "text/javascript"]],
  ["/firmware.mjs", ["./dist/firmware.mjs", "text/javascript"]],
  ["/hardware.mjs", ["./dist/hardware.mjs", "text/javascript"]],
  ["/board-profiles.mjs", ["./dist/board-profiles.mjs", "text/javascript"]],
  ["/firmware-recovery.mjs", ["./dist/firmware-recovery.mjs", "text/javascript"]],
  ["/presets/assets/index.json", ["./dist/presets/assets/index.json", "application/json"]],
  ...["kick01.wav", "snare02.wav", "kick04.wav", "voice.wav"].map((name) => [
    `/demo-samples/${name}`, [`./dist/demo-samples/${name}`, "audio/wav"],
  ]),
  ...["01-basic.json", "02-triple.json"].map((name) => [
    `/examples/${name}`, [`../firmware/patches/${name}`, "application/json"],
  ]),
]);

const server = createServer(async (request, response) => {
  const path = request.url.split("?", 1)[0];
  if (path === "/presets/index.json") {
    try {
      const files = (await readdir(new URL("./dist/presets/", import.meta.url)))
        .filter(name => name !== "index.json" && /^[a-z0-9-]+\.json$/.test(name)).sort();
      response.writeHead(200, { "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store" });
      response.end(JSON.stringify(files));
    } catch { response.writeHead(500).end(); }
    return;
  }
  const preset = /^\/presets\/([a-z0-9-]+\.json)$/.exec(path);
  const pcm = /^\/presets\/assets\/([a-f0-9]{14}\.pcm)$/.exec(path);
  const firmware = /^\/firmware\/((?:s3-32m16m\/)?[a-z0-9.-]+\.(?:json|bin))$/.exec(path);
  const factory = /^\/factory\/((?:s3-32m16m\/)?[a-z0-9.-]+\.(?:json|bin))$/.exec(path);
  const vendor = /^\/vendor\/esptool\/((?:[a-zA-Z0-9_-]+\/)*[a-zA-Z0-9_.-]+\.(?:js|json))$/.exec(path);
  const asset = preset ? [`./dist/presets/${preset[1]}`, "application/json"] :
    pcm ? [`./dist/presets/assets/${pcm[1]}`, "application/octet-stream"] :
    firmware ? [`./dist/firmware/${firmware[1]}`, firmware[1].endsWith(".json") ? "application/json" : "application/octet-stream"] :
    factory ? [`./dist/factory/${factory[1]}`, factory[1].endsWith(".json") ? "application/json" : "application/octet-stream"] :
    vendor ? [`./dist/vendor/esptool/${vendor[1]}`, vendor[1].endsWith(".json") ? "application/json" : "text/javascript"] : assets.get(path);
  if (!asset) {
    response.writeHead(404).end();
    return;
  }
  try {
    const [file, mime] = asset;
    const content = await readFile(new URL(file, import.meta.url));
    response.writeHead(200, { "Content-Type": mime + "; charset=utf-8", "Cache-Control": "no-store" });
    response.end(content);
  } catch (error) {
    console.error(error.message);
    response.writeHead(500).end();
  }
});
server.listen(8765, "127.0.0.1", () => console.log("http://127.0.0.1:8765/"));
