// Exports the product data the native client bundles, from the website's
// own sources, into native-desktop/resources/generated:
//
//   strings.json   the `native` section of src/lib/i18n.ts (ru, en)
//   palettes.json  the five palettes of src/app/signal.css, resolved
//   fandoms.json   the fandom catalogue (src/lib/fandoms.ts) with posters
//
//   node native-desktop/tools/export-resources.mjs          write files
//   node native-desktop/tools/export-resources.mjs --check  fail if stale
//
// The outputs are committed, so building the client needs no Node.js; the
// root unit tests run --check so the files cannot drift from the website.
import { existsSync } from "node:fs";
import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { build } from "esbuild";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const out = path.join(root, "native-desktop/resources/generated");

async function load(entry) {
  const temporary = path.join(root, `.native-export-${path.basename(entry)}.mjs`);
  await build({
    entryPoints: [path.join(root, entry)],
    outfile: temporary,
    bundle: true,
    format: "esm",
    platform: "neutral",
    logLevel: "silent",
    loader: { ".json": "json" },
  });
  try {
    return await import(pathToFileURL(temporary).href + "?" + Date.now());
  } finally {
    await rm(temporary, { force: true });
  }
}

const placeholders = (text) => [...text.matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort().join();

export async function exportStrings() {
  const { messages } = await load("src/lib/i18n.ts");
  const ru = messages.ru.native;
  const en = messages.en.native;
  const keys = Object.keys(ru).sort();
  if (keys.join() !== Object.keys(en).sort().join())
    throw new Error("native strings: the two locales have different keys");
  for (const key of keys) {
    for (const [locale, table] of [["ru", ru], ["en", en]])
      if (typeof table[key] !== "string" || !table[key].trim())
        throw new Error(`native strings: ${locale}.${key} is not a non-empty string`);
    if (placeholders(ru[key]) !== placeholders(en[key]))
      throw new Error(`native strings: placeholders differ for ${key}`);
  }
  const sorted = (table) => Object.fromEntries(keys.map((key) => [key, table[key]]));
  return { ru: sorted(ru), en: sorted(en) };
}

// Exactly these tokens are exported; each must be a hex colour.
const TOKENS = [
  "background", "surface", "surface-elevated", "surface-hover", "foreground",
  "muted-foreground", "border", "border-strong", "primary",
  "primary-foreground", "accent", "focus-ring", "selected", "success",
  "warning", "danger", "info", "overlay", "shadow",
];
const THEMES = ["obsidian", "ember", "aurora", "violet", "paper"];

/**
 * Supported syntax, checked strictly: a rule whose selector list contains
 * `[data-theme="<name>"]`, and inside it `--token: #hex;` declarations.
 * Other palettes start from Obsidian, as the cascade does on the website.
 */
export async function exportPalettes() {
  const css = (await readFile(path.join(root, "src/app/signal.css"), "utf8")).replaceAll("\r\n", "\n");
  const blocks = {};
  for (const theme of THEMES) {
    const selector = `[data-theme="${theme}"] {`;
    const start = css.indexOf(selector);
    if (start < 0) throw new Error(`palette ${theme} not found in signal.css`);
    const end = css.indexOf("\n}", start);
    const body = css.slice(start + selector.length, end);
    const values = {};
    for (const match of body.matchAll(/^\s*--([a-z-]+):\s*([^;]+);/gm))
      values[match[1]] = match[2].trim();
    const scheme = /color-scheme:\s*(dark|light)/.exec(body)?.[1];
    if (!scheme) throw new Error(`palette ${theme}: no color-scheme`);
    blocks[theme] = { scheme, values };
  }
  const palettes = {};
  for (const theme of THEMES) {
    const merged = { ...blocks.obsidian.values, ...blocks[theme].values };
    const colors = {};
    for (const token of TOKENS) {
      const value = merged[token];
      if (!/^#(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(value ?? ""))
        throw new Error(`palette ${theme}: --${token} is not a hex colour (${value})`);
      colors[token.replace(/-([a-z])/g, (_, c) => c.toUpperCase())] = toQtColor(value);
    }
    palettes[theme] = { scheme: blocks[theme].scheme, colors };
  }
  return palettes;
}

/** CSS #rrggbbaa → Qt #aarrggbb (Qt reads 8-digit hex alpha first). */
function toQtColor(value) {
  let hex = value.slice(1).toLowerCase();
  if (hex.length <= 4) hex = [...hex].map((c) => c + c).join("");
  return hex.length === 8 ? `#${hex.slice(6)}${hex.slice(0, 6)}` : `#${hex}`;
}

export async function exportFandoms() {
  const { fandoms } = await load("src/lib/fandoms.ts");
  const posters = JSON.parse(await readFile(path.join(root, "src/data/fandom-posters.json"), "utf8"));
  const byId = new Map(posters.map((poster) => [poster.fandomId, poster]));
  return fandoms.map((fandom) => {
    const poster = byId.get(fandom.id);
    let art = null;
    if (poster?.src?.startsWith("/images/fandoms/")) {
      const file = poster.src.slice("/images/fandoms/".length);
      if (!/^[a-z]+-\d+(?:-poster(?:-480)?)?\.webp$/.test(file))
        throw new Error(`fandom ${fandom.id}: unexpected poster name ${file}`);
      if (!existsSync(path.join(root, "public/images/fandoms", file)))
        throw new Error(`fandom ${fandom.id}: poster ${file} is missing`);
      art = { file, width: poster.width, height: poster.height };
    }
    return { id: fandom.id, name: fandom.name, category: fandom.category, art };
  });
}

const json = (value) => JSON.stringify(value, null, 1) + "\n";

export async function exportAll() {
  return {
    "strings.json": json(await exportStrings()),
    "palettes.json": json(await exportPalettes()),
    "fandoms.json": json(await exportFandoms()),
  };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const files = await exportAll();
  if (process.argv.includes("--check")) {
    const stale = [];
    for (const [name, content] of Object.entries(files)) {
      const current = existsSync(path.join(out, name))
        ? (await readFile(path.join(out, name), "utf8")).replaceAll("\r\n", "\n")
        : null;
      if (current !== content) stale.push(name);
    }
    if (stale.length) {
      console.error(`Stale native resources: ${stale.join(", ")}. Run npm run native:export.`);
      process.exit(1);
    }
    console.log("Native resources are up to date.");
  } else {
    await mkdir(out, { recursive: true });
    for (const [name, content] of Object.entries(files))
      await writeFile(path.join(out, name), content);
    console.log(`Wrote ${Object.keys(files).join(", ")} to native-desktop/resources/generated.`);
  }
}
