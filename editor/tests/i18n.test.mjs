import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { loadLanguage, tr } from "../dist/i18n.mjs";
import { factoryConsentValid } from "../dist/factory-install.mjs";

const codes = ["en", "es", "zh", "de", "it", "pt", "ja", "ko"];
let keys;
for (const code of codes) {
  await loadLanguage(code, async url => {
    const catalog = JSON.parse(await readFile(url, "utf8"));
    keys ??= Object.keys(catalog);
    assert.deepEqual(Object.keys(catalog), keys, code);
    for (const [source, translation] of Object.entries(catalog)) {
      const fields = text => [...text.matchAll(/\{\d+\}/g)].map(match => match[0]).sort();
      assert.ok(translation.trim(), source);
      assert.deepEqual(fields(translation), fields(source), source);
    }
    return { ok: true, json: async () => catalog };
  });
  const phrase = tr("EFFACER");
  assert.ok(tr("Pour confirmer, saisis EFFACER").includes(phrase), code);
  assert.ok(tr("Coche l’avertissement et saisis EFFACER avant l’installation.").includes(phrase), code);
  assert.ok(factoryConsentValid(true, phrase), code);
  assert.ok(!factoryConsentValid(false, phrase), code);
  assert.ok(!factoryConsentValid(true, "EFFACER"), code);
}
await loadLanguage("fr");
assert.ok(factoryConsentValid(true, "EFFACER"));
