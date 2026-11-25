import { tr } from "./i18n.mjs";
import { fetchScreenBands, sendScreenKey } from "./protocol.mjs?v=21";

const headerBytes = 6;
const lastBand = 255;
const queueLimit = 16;

function geometry(capability) {
  const { width, height, bandHeight } = capability ?? {};
  if (![width, height, bandHeight].every(Number.isInteger) || width < 1 || width > 320 || height < 1 ||
      height > 320 || bandHeight < 1 || bandHeight > 16 || height % bandHeight || height / bandHeight >= lastBand) return;
  const pixelsPerBand = width * bandHeight;
  return { width, height, bands: height / bandHeight, pixelsPerBand, bytesPerBand: 1 + 2 * pixelsPerBand };
}

function applyBands(data, frame, firstBand, full, spec) {
  const invalid = () => new Error(tr("Réponse série invalide."));
  if (data.length < headerBytes || data.length !== headerBytes + data[5] * spec.bytesPerBand) throw invalid();
  const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const revision = view.getUint32(0, true);
  const next = data[4];
  if (!revision || (next !== lastBand && (next <= firstBand || next >= spec.bands))) throw invalid();
  let minimum = firstBand;
  for (let offset = headerBytes; offset < data.length; offset += spec.bytesPerBand) {
    const band = data[offset];
    if (band < minimum || band >= Math.min(next, spec.bands) || (full && band !== minimum)) throw invalid();
    minimum = band + 1;
    for (let i = 0, pixel = 4 * band * spec.pixelsPerBand; i < spec.pixelsPerBand; i++, pixel += 4) {
      const color = view.getUint16(offset + 1 + i * 2); // LCD wire order, high byte first.
      const red = color >> 11, green = (color >> 5) & 63, blue = color & 31;
      frame.data[pixel] = (red << 3) | (red >> 2);
      frame.data[pixel + 1] = (green << 2) | (green >> 4);
      frame.data[pixel + 2] = (blue << 3) | (blue >> 2);
      frame.data[pixel + 3] = 255;
    }
  }
  if (full && minimum !== Math.min(next, spec.bands)) throw invalid();
  return { revision, next, changed: data[5] > 0 };
}

export function initializeDeviceScreen(panel) {
  const canvas = panel.querySelector("canvas");
  const context = canvas.getContext("2d", { alpha: false });
  const status = panel.querySelector("[role=status]");
  const controls = [...panel.querySelectorAll("[data-screen-key]")];
  const encoder = panel.querySelector("[data-screen-key=push]");
  const keys = [];
  let current;
  let capability;
  let visible = false, paused = false, timer, inflight;

  const isActive = () => Boolean(current?.spec && visible && !paused && !document.hidden);

  function render() {
    const message = !current ? tr("Connecte le Modulox en USB pour voir son écran.")
      : !current.spec ? tr("Pour synchroniser l’écran, vérifie que le firmware du Modulox est à jour.")
      : paused ? tr("Transfert USB en cours…")
      : current.error || current.keyError || (current.ready ? tr("Écran synchronisé") : tr("Synchronisation de l’écran…"));
    if (status.textContent !== message) status.textContent = message;
    canvas.hidden = !current?.ready;
    for (const button of controls) button.disabled = !isActive() || !current.ready || Boolean(current.error);
  }

  function schedule(delay) {
    clearTimeout(timer);
    if (isActive() && !inflight) timer = setTimeout(tick, delay);
  }

  async function tick() {
    if (!isActive() || inflight) return;
    const session = current;
    const key = keys.shift();
    const maxBytes = headerBytes + (session.spec.bands - session.firstBand) * session.spec.bytesPerBand;
    const work = key ? sendScreenKey(session.port, key)
      : fetchScreenBands(session.port, session.since, session.firstBand, maxBytes);
    inflight = work;
    try {
      const data = await work;
      if (session !== current) return;
      if (key) {
        session.keyError = "";
      } else {
        const { revision, next, changed } = applyBands(data, session.frame, session.firstBand, !session.since, session.spec);
        // Only acknowledge the first revision in a scan. An update behind its
        // cursor will then be read in the next scan, even if later replies saw it.
        if (session.firstBand === 0) session.anchor = revision;
        session.dirty ||= changed;
        if (next === lastBand) {
          if (session.dirty) context.putImageData(session.frame, 0, 0);
          session.ready = true;
          session.dirty = false;
          session.since = session.anchor;
          session.firstBand = 0;
        } else session.firstBand = next;
        session.error = "";
      }
    } catch (error) {
      if (key) session.keyError = error.message;
      else {
        session.since = session.firstBand = 0;
        session.error = error.message;
      }
    } finally {
      inflight = undefined;
      render();
      const delay = current?.error ? 450 : keys.length || current?.firstBand ? 0 : key ? 30 : 100;
      schedule(delay);
    }
  }

  function press(key) {
    if (!isActive() || !current.ready || current.error || keys.length >= queueLimit) return;
    keys.push(key);
    schedule(0);
  }

  for (const button of controls) button.addEventListener("click", () => press(button.dataset.screenKey));
  encoder.addEventListener("wheel", event => {
    if (encoder.disabled || !event.deltaY) return;
    event.preventDefault();
    press(event.deltaY > 0 ? "cw" : "ccw");
  }, { passive: false });
  encoder.addEventListener("keydown", event => {
    if (!["ArrowLeft", "ArrowRight", "ArrowDown", "ArrowUp"].includes(event.key)) return;
    event.preventDefault();
    press(["ArrowRight", "ArrowDown"].includes(event.key) ? "cw" : "ccw");
  });
  document.addEventListener("visibilitychange", () => {
    if (!isActive()) keys.length = 0;
    render();
    schedule(0);
  });

  return {
    update(port, mirror, shown, busy) {
      if (port !== current?.port || mirror !== capability) {
        capability = mirror;
        const spec = geometry(mirror);
        current = port ? { port, spec, since: 0, firstBand: 0 } : undefined;
        if (current?.spec) {
          canvas.width = spec.width;
          canvas.height = spec.height;
          current.frame = context.createImageData(spec.width, spec.height);
        }
        keys.length = 0;
      }
      visible = shown;
      paused = busy;
      if (!isActive()) keys.length = 0;
      render();
      schedule(0);
    },
    async idle() { await inflight?.catch(() => {}); },
  };
}
