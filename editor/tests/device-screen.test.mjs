import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { setTimeout as delay } from "node:timers/promises";
import { initializeDeviceScreen } from "../dist/device-screen.mjs";

const doc = new EventTarget();
doc.hidden = false;
globalThis.document = doc;
const frames = [];
const context = {
  createImageData: (width, height) => ({ data: new Uint8ClampedArray(width * height * 4) }),
  putImageData: frame => frames.push([...frame.data]),
};
const canvas = { getContext: () => context };
const status = {};
const buttons = ["menu", "push"].map(screenKey => Object.assign(new EventTarget(), { dataset: { screenKey } }));
const screen = initializeDeviceScreen({
  querySelector: selector => selector === "canvas" ? canvas : selector === "[role=status]" ? status : buttons[1],
  querySelectorAll: () => buttons,
});

function bandReply(revision, next, pixels, ...bands) {
  const bytesPerBand = 1 + pixels * 2;
  const value = new Uint8Array(6 + bands.length * bytesPerBand);
  const view = new DataView(value.buffer);
  view.setUint32(0, revision, true);
  value.set([next, bands.length], 4);
  for (const [i, [band, color]] of bands.entries()) {
    const start = 6 + bytesPerBand * i;
    value[start] = band;
    for (let pixel = 0; pixel < pixels; pixel++) view.setUint16(start + 1 + pixel * 2, color);
  }
  return value;
}

const commands = [];
const encoder = new TextEncoder(), decoder = new TextDecoder();
let controller, finishRead;
let captureReply = false;
const send = (body = new Uint8Array()) => {
  controller.enqueue(encoder.encode(`OK ${body.length}\n`));
  if (body.length) { controller.enqueue(body.subarray(0, 3)); controller.enqueue(body.subarray(3)); }
};
const port = {
  readable: new ReadableStream({ start(source) { controller = source; } }),
  writable: new WritableStream({ write(bytes) {
    const command = decoder.decode(bytes).trim();
    commands.push(command);
    switch (command) {
      case "SCREEN 0 0": send(bandReply(10, 1, 2, [0, 0xf800])); break;
      case "SCREEN 0 1": send(bandReply(11, 255, 2, [1, 0x001f])); break;
      case "SCREEN 10 0": send(bandReply(11, 255, 2, [0, 0x07e0])); break;
      case "SCREEN 11 0":
        if (captureReply) finishRead = () => { finishRead = undefined; send(bandReply(11, 255, 2)); };
        else send(bandReply(11, 255, 2));
        break;
      case "UIKEY menu":
      case "UIKEY push": send(); break;
      default: throw new Error(`Unexpected command ${command}`);
    }
  } }),
};

async function until(condition) {
  for (let i = 0; i < 200 && !condition(); i++) await delay(10);
  assert.ok(condition(), "Timed out waiting for the screen");
}

const mirror = { width: 2, height: 2, bandHeight: 1 };
try {
  screen.update(port, undefined, true, false);
  assert.ok(buttons.every(button => button.disabled));
  assert.match(status.textContent, /firmware/);
  assert.equal(commands.length, 0);
  screen.update(port, mirror, true, false);
  await until(() => frames.length === 2);
  assert.deepEqual(commands.slice(0, 3), ["SCREEN 0 0", "SCREEN 0 1", "SCREEN 10 0"]);
  assert.deepEqual(frames[0], [255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255]);
  assert.deepEqual(frames[1].slice(0, 4), [0, 255, 0, 255]);
  assert.equal(canvas.hidden, false);
  assert.ok(buttons.every(button => !button.disabled));

  buttons[0].dispatchEvent(new Event("click"));
  buttons[1].dispatchEvent(new Event("click"));
  await until(() => commands.includes("UIKEY push"));
  assert.deepEqual(commands.filter(command => command.startsWith("UIKEY")), ["UIKEY menu", "UIKEY push"]);

  captureReply = true;
  await until(() => finishRead);
  screen.update(port, mirror, true, true);
  assert.ok(buttons.every(button => button.disabled));
  assert.ok(port.readable.locked);
  const idle = screen.idle();
  finishRead();
  await idle;
  assert.ok(!port.readable.locked && !port.writable.locked);
  const pausedCount = commands.length;
  await delay(140);
  assert.equal(commands.length, pausedCount);
} finally {
  captureReply = false;
  finishRead?.();
  screen.update(undefined, undefined, false, false);
}

const dist = new URL("../dist/", import.meta.url);
async function protocolImport(name) {
  const source = await readFile(new URL(name, dist), "utf8");
  return source.match(/from "(\.\/protocol\.mjs[^\"]*)"/)?.[1];
}
const appProtocol = await protocolImport("app.mjs");
assert.equal(await protocolImport("factory-bank.mjs"), appProtocol);
const { fetchResources } = await import(new URL(appProtocol, dist));

// Firmware 1.7 fits five LCD bands in each 24 KiB response. A telemetry
// request arriving during that transfer must wait for the same serial port.
const liveCommands = [];
let liveController, finishFirstBand;
const sendLive = body => {
  liveController.enqueue(encoder.encode(`OK ${body.length}\n`));
  for (let offset = 0; offset < body.length; offset += 512) liveController.enqueue(body.subarray(offset, offset + 512));
};
const livePort = {
  readable: new ReadableStream({ start(source) { liveController = source; } }),
  writable: new WritableStream({ write(bytes) {
    const command = decoder.decode(bytes).trim();
    liveCommands.push(command);
    if (command === "RESOURCES") { sendLive(encoder.encode('{"freeBytes":12345}')); return; }
    const [verb, revision, first] = command.split(" ");
    assert.equal(verb, "SCREEN");
    if (revision !== "0") { sendLive(bandReply(1, 255, 2240)); return; }
    const firstBand = Number(first);
    const next = firstBand + 5;
    const bands = Array.from({ length: 5 }, (_, band) => [firstBand + band, 0x07e0]);
    const body = bandReply(1, next === 30 ? 255 : next, 2240, ...bands);
    assert.equal(body.length, 22411);
    if (firstBand === 0) finishFirstBand = () => { finishFirstBand = undefined; sendLive(body); };
    else sendLive(body);
  } }),
};

try {
  const frameCount = frames.length;
  screen.update(livePort, { width: 280, height: 240, bandHeight: 8 }, true, false);
  await until(() => finishFirstBand);
  const resources = fetchResources(livePort);
  finishFirstBand();
  assert.deepEqual(await resources, { freeBytes: 12345 });
  assert.deepEqual(liveCommands.slice(0, 2), ["SCREEN 0 0", "RESOURCES"]);
  await until(() => frames.length > frameCount);
  assert.equal(status.textContent, "Écran synchronisé");
  assert.equal(frames.at(-1).length, 280 * 240 * 4);
  assert.deepEqual(frames.at(-1).slice(-4), [0, 255, 0, 255]);
} finally {
  finishFirstBand?.();
  screen.update(undefined, undefined, false, false);
  await screen.idle();
}
