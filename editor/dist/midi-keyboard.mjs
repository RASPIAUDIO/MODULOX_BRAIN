import { tr } from "./i18n.mjs";

const noteNames = ["C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"];
const shortcuts = [
  "KeyZ", "KeyS", "KeyX", "KeyD", "KeyC", "KeyV", "KeyG", "KeyB", "KeyH", "KeyN", "KeyJ", "KeyM",
  "KeyQ", "Digit2", "KeyW", "Digit3", "KeyE", "KeyR", "Digit5", "KeyT", "Digit6", "KeyY", "Digit7", "KeyU", "KeyI",
];
const keyOffsets = new Map(shortcuts.map((code, index) => [code, index]));
keyOffsets.set("Comma", 12);

export function moduloxOutputs(outputs) {
  return [...outputs].filter(port => port.state === "connected" && /\bmodulox\b/i.test(port.name ?? ""));
}

export function createNotePlayer(onChange) {
  const inputs = new Map();
  const counts = new Map();
  let output;
  let channel = 0;

  function send(status, note, velocity) {
    try {
      if (output?.state !== "connected") return false;
      output.send([status | channel, note, velocity]);
      return true;
    } catch {
      return false;
    }
  }

  function releaseAll() {
    for (const note of counts.keys()) {
      send(0x80, note, 0);
      onChange(note, false);
    }
    inputs.clear();
    counts.clear();
  }

  return {
    configure(port, nextChannel) {
      if (port === output && nextChannel === channel) return;
      releaseAll();
      output = port;
      channel = nextChannel;
    },
    press(input, note) {
      if (inputs.has(input)) return false;
      const count = counts.get(note) ?? 0;
      if (count === 0 && !send(0x90, note, 96)) return false;
      inputs.set(input, note);
      counts.set(note, count + 1);
      if (count === 0) onChange(note, true);
      return true;
    },
    release(input) {
      if (!inputs.has(input)) return;
      const note = inputs.get(input);
      inputs.delete(input);
      const count = counts.get(note) - 1;
      if (count > 0) {
        counts.set(note, count);
        return;
      }
      counts.delete(note);
      send(0x80, note, 0);
      onChange(note, false);
    },
    releaseAll,
  };
}

export function initializeMidiKeyboard(drawer) {
  const piano = drawer.querySelector("#midi-piano");
  const connect = drawer.querySelector("#midi-enable");
  const message = drawer.querySelector("#midi-status");
  const portLabel = drawer.querySelector("#midi-port-label");
  const ports = drawer.querySelector("#midi-port");
  const channel = drawer.querySelector("#midi-channel");
  const octaveDown = drawer.querySelector("#midi-octave-down");
  const octaveUp = drawer.querySelector("#midi-octave-up");
  const octaveLabel = drawer.querySelector("#midi-octaves");
  let access;
  let output;
  let octave = 3;
  let layout;
  let suspended = false;
  let requesting = false;
  const startNote = () => 12 * (octave + 1);
  const player = createNotePlayer((note, held) => {
    keys[note - startNote()]?.toggleAttribute("data-held", held);
  });

  let whiteKeys = 0;
  const keys = shortcuts.map((_code, offset) => {
    const black = noteNames[offset % 12].endsWith("♯");
    const key = document.createElement("button");
    key.type = "button";
    key.className = black ? "piano-key black-key" : "piano-key white-key";
    key.style.left = (100 * whiteKeys / 15) + "%";
    if (!black) whiteKeys++;
    key.addEventListener("pointerdown", event => {
      if (event.button === 0 && press(event.pointerId, offset)) key.setPointerCapture(event.pointerId);
    });
    for (const eventName of ["pointerup", "pointercancel", "lostpointercapture"]) {
      key.addEventListener(eventName, event => player.release(event.pointerId));
    }
    key.addEventListener("click", event => {
      if (event.detail !== 0) return;
      const input = Symbol();
      if (press(input, offset)) setTimeout(() => player.release(input), 160);
    });
    piano.append(key);
    return key;
  });

  for (let number = 1; number <= 16; number++) channel.add(new Option(number, number - 1));

  function updateLabels() {
    octaveLabel.textContent = "C" + octave + "–C" + (octave + 2);
    keys.forEach((key, offset) => {
      const note = noteNames[offset % 12] + (octave + Math.floor(offset / 12));
      const code = shortcuts[offset];
      key.setAttribute("aria-label", note);
      key.title = note;
      key.textContent = (layout?.get(code) ?? code.replace(/^(Key|Digit)/, "")).toUpperCase();
    });
  }

  function press(input, offset) {
    return drawer.open && !suspended && player.press(input, startNote() + offset);
  }

  function renderControls() {
    const disabled = suspended || requesting;
    connect.disabled = disabled || typeof navigator.requestMIDIAccess !== "function";
    ports.disabled = channel.disabled = disabled;
    octaveDown.disabled = disabled || octave <= 0;
    octaveUp.disabled = disabled || octave >= 7;
    keys.forEach(key => { key.disabled = disabled || !output; });
  }

  function selectOutput(port) {
    output = port;
    player.configure(output, Number(channel.value));
    renderControls();
  }

  function refreshOutputs() {
    const candidates = moduloxOutputs(access.outputs.values());
    const selected = candidates.find(port => port.id === ports.value) ?? (candidates.length === 1 ? candidates[0] : undefined);
    ports.replaceChildren(new Option(tr("Choisir un Modulox"), ""));
    for (const port of candidates) ports.add(new Option(port.name, port.id));
    ports.value = selected?.id ?? "";
    portLabel.hidden = candidates.length < 2;
    selectOutput(selected);
    if (!candidates.length) message.textContent = tr("Branche le Modulox en USB et démarre-le normalement.");
    else if (!selected) message.textContent = tr("Choisis le Modulox à jouer.");
    else message.textContent = tr("MIDI connecté à {0}.", selected.name);
  }

  connect.onclick = async () => {
    requesting = true;
    renderControls();
    try {
      access = await navigator.requestMIDIAccess();
      access.addEventListener("statechange", refreshOutputs);
      connect.hidden = true;
      refreshOutputs();
    } catch {
      message.textContent = tr("Accès MIDI indisponible. Vérifie l’autorisation MIDI du site.");
    } finally {
      requesting = false;
      renderControls();
    }
  };

  ports.onchange = refreshOutputs;
  channel.onchange = () => player.configure(output, Number(channel.value));
  octaveDown.onclick = () => shiftOctave(-1);
  octaveUp.onclick = () => shiftOctave(1);

  function shiftOctave(amount) {
    player.releaseAll();
    octave += amount;
    updateLabels();
    renderControls();
  }

  document.addEventListener("keydown", event => {
    const offset = keyOffsets.get(event.code);
    if (offset === undefined || event.defaultPrevented || event.isComposing || event.ctrlKey || event.altKey || event.metaKey ||
        !drawer.open || !output || suspended || document.querySelector("dialog[open]") ||
        event.target.closest?.("input, textarea, select, [contenteditable]")) return;
    event.preventDefault();
    if (!event.repeat) press(event.code, offset);
  });
  document.addEventListener("keyup", event => player.release(event.code));
  window.addEventListener("blur", player.releaseAll);
  window.addEventListener("pagehide", player.releaseAll);
  document.addEventListener("visibilitychange", () => { if (document.hidden) player.releaseAll(); });
  drawer.addEventListener("toggle", () => { if (!drawer.open) player.releaseAll(); });
  new ResizeObserver(() => {
    document.documentElement.style.setProperty("--midi-drawer-height", drawer.offsetHeight + "px");
  }).observe(drawer);
  navigator.keyboard?.getLayoutMap?.().then(map => {
    layout = map;
    updateLabels();
  }).catch(() => {});
  if (typeof navigator.requestMIDIAccess !== "function") {
    connect.hidden = true;
    message.textContent = tr("Ouvre le configurateur dans Chrome ou Edge pour utiliser le MIDI USB.");
  }
  updateLabels();
  renderControls();

  return {
    setBusy(value) {
      if (suspended === value) return;
      suspended = value;
      if (suspended) player.releaseAll();
      renderControls();
    },
  };
}
