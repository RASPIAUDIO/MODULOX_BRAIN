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

export function preferredLanguage(requested, saved, browserLanguages) {
  for (const locale of [requested, saved, ...browserLanguages]) {
    const code = locale?.toLowerCase().split("-")[0];
    if (Object.hasOwn(languages, code)) return code;
  }
  return "fr";
}

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
  let code = preferredLanguage(url.searchParams.get("lang"), saved, [...navigator.languages, navigator.language]);
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
  const trigger = picker.querySelector("summary");
  const options = picker.querySelector(".language-menu");
  const [, activeFlag, activeName] = languages[code];
  trigger.setAttribute("aria-label", tr("Langue") + " · " + activeName);
  trigger.title = activeName;
  trigger.append(flagIcon(activeFlag));
  for (const [language, [tag, flag, name]] of Object.entries(languages)) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.language = language;
    button.lang = tag;
    button.setAttribute("aria-pressed", String(language === code));
    button.append(flagIcon(flag), name);
    button.onclick = () => {
      try { localStorage.setItem(preference, language); } catch {}
      if (language === code) {
        picker.open = false;
        trigger.focus();
        return;
      }
      const next = new URL(location.href);
      next.searchParams.set("lang", language);
      location.assign(next);
    };
    options.append(button);
  }
  picker.addEventListener("keydown", event => {
    if (event.key !== "Escape" || !picker.open) return;
    event.preventDefault();
    picker.open = false;
    trigger.focus();
  });
  document.addEventListener("pointerdown", event => {
    if (picker.open && !picker.contains(event.target)) picker.open = false;
  });
  picker.hidden = false;
}

function flagIcon(flag) {
  const icon = new Image(20, 15);
  icon.alt = "";
  icon.src = new URL(`./flags/${flag}.png`, import.meta.url);
  return icon;
}
