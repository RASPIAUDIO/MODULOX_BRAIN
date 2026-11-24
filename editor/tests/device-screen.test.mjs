import assert from "node:assert/strict";
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

function bandReply(revision, next, ...bands) {
  const value = new Uint8Array(6 + bands.length * 5);
  const view = new DataView(value.buffer);
  view.setUint32(0, revision, true);
  value.set([next, bands.length], 4);
  for (const [i, [band, color]] of bands.entries()) {
    value[6 + 5 * i] = band;
    view.setUint16(7 + 5 * i, color);
    view.setUint16(9 + 5 * i, color);
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
      case "SCREEN 0 0": send(bandReply(10, 1, [0, 0xf800])); break;
      case "SCREEN 0 1": send(bandReply(11, 255, [1, 0x001f])); break;
      case "SCREEN 10 0": send(bandReply(11, 255, [0, 0x07e0])); break;
      case "SCREEN 11 0":
        if (captureReply) finishRead = () => { finishRead = undefined; send(bandReply(11, 255)); };
        else send(bandReply(11, 255));
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
