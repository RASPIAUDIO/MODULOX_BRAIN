import assert from "node:assert/strict";
import { createNotePlayer, moduloxOutputs } from "../dist/midi-keyboard.mjs";

const messages = [];
const output = { name: "2- Modulox Sine MIDI", state: "connected", send: bytes => messages.push(bytes) };
assert.deepEqual(moduloxOutputs([
  { name: "Other USB Synth", state: "connected" },
  { name: "Modulox Sine MIDI", state: "disconnected" },
  output,
]), [output]);

const active = new Set();
const player = createNotePlayer((note, held) => held ? active.add(note) : active.delete(note));
assert.equal(player.press("KeyZ", 48), false);
player.configure(output, 3);
assert.equal(player.press("KeyZ", 48), true);
assert.equal(player.press("KeyZ", 48), false);
assert.equal(player.press(1, 48), true);
player.release("KeyZ");
assert.deepEqual(messages, [[0x93, 48, 96]]);
assert.deepEqual([...active], [48]);
player.release(1);
assert.deepEqual(messages.pop(), [0x83, 48, 0]);
assert.equal(active.size, 0);

player.press("KeyZ", 48);
player.press("KeyX", 50);
player.configure(output, 0);
assert.deepEqual(messages.slice(-4), [[0x93, 48, 96], [0x93, 50, 96], [0x83, 48, 0], [0x83, 50, 0]]);
assert.equal(active.size, 0);
player.press("KeyQ", 60);
player.releaseAll();
assert.deepEqual(messages.slice(-2), [[0x90, 60, 96], [0x80, 60, 0]]);
assert.equal(active.size, 0);

output.state = "disconnected";
assert.equal(player.press("KeyZ", 48), false);
assert.equal(active.size, 0);
