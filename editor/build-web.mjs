import { copyFile, mkdir, readdir, writeFile } from "node:fs/promises";

const presets = new URL("./dist/presets/", import.meta.url);
const examples = new URL("./dist/examples/", import.meta.url);
const files = (await readdir(presets, { withFileTypes: true }))
  .filter(file => file.isFile() && file.name !== "index.json" && /^[a-z0-9-]+\.json$/.test(file.name))
  .map(file => file.name)
  .sort();

await mkdir(examples, { recursive: true });
for (const name of ["01-basic.json", "02-triple.json"]) {
  await copyFile(new URL("../firmware/patches/" + name, import.meta.url), new URL(name, examples));
}
await writeFile(new URL("index.json", presets), JSON.stringify(files, null, 2) + "\n");
