import assert from "node:assert/strict";
import { estimateResources } from "../dist/resources.mjs";
import { loadLanguage, tr } from "../dist/i18n.mjs";
import vm from "node:vm";
import { readFile } from "node:fs/promises";
import { offlineCatalog, moduleCategories, modulePaletteGroups } from "../dist/patch.mjs";

// Every shipped module is reachable once; older and future catalogs also work.
const groups = modulePaletteGroups(offlineCatalog);
const types = groups.flatMap(group => group.modules.map(spec => spec.type));
assert.equal(new Set(types).size, types.length);
assert.deepEqual([...types].sort(), offlineCatalog.modules.map(spec => spec.type).sort());
assert.equal(groups.length, 6);
assert.deepEqual(modulePaletteGroups({ modules: [] }), []);
assert.deepEqual(modulePaletteGroups({ modules: [{ type: "Future", limit: 1 }] }).map(g => g.id), ["other"]);
assert.deepEqual(modulePaletteGroups({ modules: [offlineCatalog.modules.find(s => s.type === "Filter")] }).map(g => g.id), ["effects"]);
assert.equal(moduleCategories.find(g => g.id === "time").types.includes("Reverb"), true);

// Run the actual palette renderer: category changes, instance limits, click,
// drag payload and selection persistence when the rest of the editor rerenders.
const source = await readFile(new URL("../dist/app.mjs", import.meta.url), "utf8");
const code = source.slice(source.indexOf("function renderPalette()"), source.indexOf("function layout(patch)"));
function element(tag, text = "", className = "") {
  return { tag, text, className, children: [], dataset: {}, attributes: {},
    append(child) { this.children.push(child); }, replaceChildren() { this.children = []; },
    setAttribute(key, value) { this.attributes[key] = value; },
    focus() { this.focused = true; },
    querySelector(selector) { const id = /data-category="(.*?)"/.exec(selector)[1]; return this.children.find(c => c.dataset.category === id); },
  };
}
const nodes = new Map(["palette", "module-categories"].map(id => [id, element("div")]));
const patch = { connections: [], modules: [{ id: "osc", type: "Osc" }, { id: "rev", type: "Reverb" }] };
const added = [];
const context = { selectedModuleCategory: "generators", catalog: offlineCatalog, element, tr,
  $: id => nodes.get(id), activeDraft: () => ({ patch }), modulePaletteGroups,
  resourceError: patch => estimateResources(patch, offlineCatalog).errors[0] ?? "",
  resourceBudget: patch => estimateResources(patch, offlineCatalog), bytesLabel: bytes => String(bytes),
  samplerUnavailable: () => "", addModule: type => added.push(type) };
vm.runInNewContext(code, context);
context.renderPalette();
const palette = nodes.get("palette"), categories = nodes.get("module-categories");
assert.equal(categories.children.length, 6);
assert.deepEqual(palette.children.map(b => b.text), moduleCategories[0].types);
assert.equal(palette.children[0].children[0].text, "1/3");
palette.children[0].onclick(); assert.deepEqual(added, ["Osc"]);
const payload = {};
palette.children[0].ondragstart({ dataTransfer: { setData(key, value) { payload[key] = value; } } });
assert.equal(payload["application/x-modulox-module"], "Osc");
categories.children.find(b => b.dataset.category === "time").onclick();
assert.equal(categories.children.find(b => b.dataset.category === "time").focused, true);
assert.equal(palette.attributes["aria-label"], "Modules : Time effects");
assert.deepEqual(palette.children.map(b => b.text), ["Delay", "Reverb", "Chorus", "Rotary"]);
assert.equal(palette.children.find(b => b.text === "Reverb").disabled, true);
assert.equal(palette.children.find(b => b.text === "Reverb").draggable, false);
context.renderPalette(); assert.equal(context.selectedModuleCategory, "time");
context.catalog = { modules: [offlineCatalog.modules.find(s => s.type === "Filter")] };
context.renderPalette(); assert.equal(context.selectedModuleCategory, "effects");
console.log("PASS: all modules categorized once, legacy/future catalogs, category selection/focus, limits, click, drag, rerender");

// Translated labels must keep the firmware module identifiers on click/drag.
for (const language of ["en", "es", "zh", "de", "it", "pt", "ja", "ko"]) {
  await loadLanguage(language, async url => ({ ok: true, json: async () => JSON.parse(await readFile(url, "utf8")) }));
  context.catalog = offlineCatalog;
  context.selectedModuleCategory = "effects";
  context.renderPalette();
  const effects = moduleCategories.find(group => group.id === "effects").types;
  assert.deepEqual(palette.children.map(button => button.text), effects.map(tr), language);
  for (const [index, button] of palette.children.entries()) {
    button.onclick(); assert.equal(added.at(-1), effects[index], language);
    button.ondragstart({ dataTransfer: { setData(key, value) { payload[key] = value; } } });
    assert.equal(payload["application/x-modulox-module"], effects[index], language);
  }
}
await loadLanguage("fr");
assert.doesNotMatch(source, /node\.type\s*===\s*tr\(/, "Module type comparisons use stable identifiers");
