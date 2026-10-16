# Site

The project's landing page: plain HTML, CSS and JavaScript, no build step.

Preview it locally:

```bash
python -m http.server 4173 --directory site
```

Then open http://localhost:4173. Opening `index.html` straight from disk works too.

## What is on it

- **The look** follows Raycast's Windows page: near-black ground, a serif display line (Instrument Serif) over a framed film, Inter for text, interface windows on gradient cards. The earlier keycap design is kept on the branch `archive/website-keycap-2026-10-16`.
- **The picker in the hero works.** Press 1, 2 or 3 (or click a card) to open an account; it gets a taskbar button. Ctrl+click picks several, Ctrl+Alt+C hides and shows the picker, a taskbar button closes its window. It is rebuilt in HTML from `src/picker.cpp`, with the same strings and figures as the README screenshots.
- **Pictures are the app's own.** `img/tray-dark.png`, `img/icons-dark.png`, `img/themes.png` and `img/main-icon.png` come from `docs/images`. `img/hero-film.mp4` is the README hero film, encoded from the English frames rendered by `tools/hero`.
- **Fonts:** Inter, Instrument Serif and Geist Mono from Google Fonts; the interface fragments use Segoe UI, like the app.

To publish with GitHub Pages, deploy the `site` folder (Settings → Pages → GitHub Actions, with a workflow that uploads `site/` as the Pages artifact).
