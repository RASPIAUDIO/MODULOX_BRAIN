const languages = {
  fr: ["fr", "fr", "Français"],
  en: ["en", "gb", "English"],
  es: ["es", "es", "Español"],
  zh: ["zh-Hans", "cn", "简体中文"],
  de: ["de", "de", "Deutsch"],
  it: ["it", "it", "Italiano"],
  pt: ["pt", "pt", "Português"],
  ja: ["ja", "jp", "日本語"],
  ko: ["ko", "kr", "한국어"],
};
const preference = "modulox.language.v1";
let messages = {};

export function tr(source, ...values) {
  const text = Object.hasOwn(messages, source) ? messages[source] : source;
  return values.length ? text.replace(/\{(\d+)\}/g, (_, index) => String(values[index])) : text;
}

export async function loadLanguage(code, request = fetch) {
  if (!Object.hasOwn(languages, code) || code === "fr") {
    messages = {};
    return;
  }
  const response = await request(new URL(`./locales/${code}.json`, import.meta.url));
  if (!response.ok) throw new Error(`Could not load the ${code} translation`);
  messages = await response.json();
}

export async function initializeLanguage() {
  const url = new URL(location.href);
  let saved;
  try { saved = localStorage.getItem(preference); } catch {}
  let code = url.searchParams.get("lang") || saved;
  if (!Object.hasOwn(languages, code)) code = "fr";
  try {
    await loadLanguage(code);
  } catch {
    code = "fr";
    await loadLanguage(code);
  }
  document.documentElement.lang = languages[code][0];

  // Translate the original markup before the editor adds patch names or other user data.
  const walker = document.createTreeWalker(document.documentElement, NodeFilter.SHOW_TEXT);
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    const text = node.textContent.trim();
    if (text && !node.parentElement.closest("script, style, [translate=no]")) {
      node.textContent = node.textContent.replace(text, () => tr(text));
    }
  }
  for (const node of document.querySelectorAll("[aria-label], [title], [placeholder]")) {
    for (const attribute of ["aria-label", "title", "placeholder"]) {
      if (node.hasAttribute(attribute)) node.setAttribute(attribute, tr(node.getAttribute(attribute)));
    }
  }

  const picker = document.getElementById("language-picker");
  for (const [language, [tag, flag, name]] of Object.entries(languages)) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.language = language;
    button.lang = tag;
    button.title = name;
    button.setAttribute("aria-label", name);
    button.setAttribute("aria-pressed", String(language === code));
    const icon = new Image(20, 15);
    icon.alt = "";
    icon.src = new URL(`./flags/${flag}.png`, import.meta.url);
    button.append(icon);
    button.onclick = () => {
      if (language === code) return;
      try { localStorage.setItem(preference, language); } catch {}
      const next = new URL(location.href);
      next.searchParams.set("lang", language);
      location.assign(next);
    };
    picker.append(button);
  }
}
