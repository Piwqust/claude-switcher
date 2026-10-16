# Site

The project's landing page: plain HTML, CSS and JavaScript, no build step.

Preview it locally:

```bash
python -m http.server 4173 --directory site
```

Then open http://localhost:4173. Opening `index.html` straight from disk works too.

## What is on it

- **The look is the app's own.** Windows 11 Fluent surfaces, Segoe UI Variable (Inter where Windows fonts are missing), the switcher's terracotta, Work blue and Personal green. The overall dark, product-first rhythm takes its cue from Raycast's Windows page; earlier versions are kept on `archive/website-keycap-2026-10-16` and in this branch's history.
- **The hero is a desktop running the switcher.** Press 1, 2 or 3 (or click a card) to open an account: its Claude window appears and the picker hides, as with "Hide after opening an account". Ctrl+click picks several, Ctrl+Alt+C or the tray icon brings the picker back, a window's ✕ closes it, a taskbar button brings it forward. The picker is rebuilt in HTML from `src/picker.cpp`.
- **The settings window is the real one.** Its strings come from `src/settings.cpp`; the switches, the countdown and the shortcut field (it records a real key combination) all respond.
- **Pictures are the app's own.** `img/tray-dark.png`, `img/icons-dark.png`, `img/themes.png` and `img/main-icon.png` come from `docs/images`; `img/hero-film.jpg` is the social preview image.
- **Fonts:** Segoe UI Variable on Windows, Inter from Google Fonts elsewhere; Cascadia Mono for paths.

To publish with GitHub Pages, deploy the `site` folder (Settings → Pages → GitHub Actions, with a workflow that uploads `site/` as the Pages artifact).
