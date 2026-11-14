# Claude Profile Switcher: site design

Built from the utility only (src/, README, docs/, the real interface screenshots in
docs/images). The earlier website was not consulted.

## Phase 0: Brief

- **Subject:** Claude Profile Switcher 1.0, a 550 KB native C++ Windows utility. It runs several
  Claude Desktop accounts at once, each in its own window with its own sign-in, by starting
  Claude with `--user-data-dir` pointed at a separate folder per account.
- **Audience:** Windows people who keep more than one Claude account (personal and work, two
  plans, a client's org) and are tired of signing out to switch. Technical enough to care that it
  makes no network requests and never reads tokens.
- **Job:** get the exe downloaded (x64 or ARM64) and trusted. Second job: explain how it works
  honestly, including what accounts share.
- **Language:** English at `/`, Russian at `/ru/`. The utility itself is English and Russian, so
  the site is too.
- **Assets:** real interface screenshots (picker, startup countdown, tray, settings, icons,
  themes), in English and Russian. No photography needed: the product is the imagery.
- **Constraints:** unofficial, so nothing may look like Anthropic's own site: no Claude logo as
  site identity, an explicit "not affiliated" line. Facts only from the repo (75 ms, 16 MB,
  550 KB, measured against the PowerShell original on one PC with three accounts).
- **Assumptions:** the production URL stays `https://claude-switcher.angleford.co` (README
  links it). Download links point at GitHub's `releases/latest/download/…` assets.

## Phase 1: Concept

| | A. Side by side (mechanism) | B. The shortcut (process) | C. Eight colours (identity) |
|---|---|---|---|
| Idea | Nothing on the site replaces anything: every account, section and page opens *beside* the last one, the way the copies of Claude do | The site is driven like the picker: Ctrl Alt C, numbers 1 to 9, keycaps everywhere | Every section takes the next of the app's eight profile colours, assigned least-used-first like `ProfileColor()` |
| Anchor | Electron's one-copy lock is per data folder, so copies pointed at different folders run side by side | the global hotkeys and number keys | the real palette in `core.cpp` |
| Signature | hero: one window splits into three account panes that stay open; the pane under the pointer comes forward (widens) and its name widens with it on the font's width axis | pressing real keys lights keycaps | badges stamped as you scroll |
| O / F / Fe / P | 4 / 5 / 4 / 2 | 3 / 4 / 4 / 3 | 3 / 4 / 5 / 3 |

**Pick A.** It is the product's actual mechanism and its promise in one picture, and it gives
rules for layout (panes), motion (open beside), transitions and the 404. Stolen details: from
B, the number keys bring a pane forward and the shortcut section's keycaps answer real key
presses; from C, the panes use the app's real profile palette.

```
Idea:        Every account in its own window, all open at once: the site opens things beside each other, never instead.
Metaphor:    a Windows desktop where copies of one app stand side by side, each signed in.
Signature:   the hero split. One pane opens, then two more open beside it; the pane under the
             pointer (or the number key pressed) comes forward and its name widens on the
             Roboto Flex width axis. Driver module: src/motion/panes.ts.
Emotion:     relief and control: "oh, they are all just open".
Motion verb: open beside. Things arrive from the right edge of their container and the
             neighbours make room; nothing fades out to be replaced.
Screenshot:  three coloured account windows with green "open" dots, the headline running
             across all three behind the window gaps.
```

## Phase 2: Art direction

### Palette (from the app's own theme and profile colours, gfx.cpp and core.cpp)

| Role | Hex | Source |
|---|---|---|
| ground | `#1b1b1b` | a shade under the app's dark window `#202020`, so panes read as windows on a desktop |
| raised | `#262626` | card surfaces, like the app's `#2C2C2C` cards |
| ink | `#f3f3f3` | the app's light-theme ground, used as text |
| muted | `#a3a3a3` | secondary text, 6.8:1 on ground |
| line | `rgb(243 243 243 / .12)` | hairlines and dashed "Add" outlines |
| open | `#6ccb5f` | the app's "Open" green. Only for the open-state dot and the current page |
| pane ink | `#141414` | text on coloured panes |
| main pane | `#e08a6b` | the app's accent, which it uses for the Main account |
| account panes | `#5596c4` `#5fa872` `#a275c2` `#c8607e` `#4fa6a6` | the app's profile palette, lifted 10% so dark ink passes AA |

No gradients. Colour appears only where an account appears.

### Type

- **Roboto Flex** (Google, OFL, variable `wght 100..1000`, `wdth 25..151`, Latin + Cyrillic):
  display and text. Display at weight 720, width 112 to 151, tracking -0.04em, leading 0.88.
  Account names in panes ride the width axis with the pane's width (condensed when narrow, wide
  when forward). Text at weight 400, width 100.
- **JetBrains Mono** (Google, OFL, Latin + Cyrillic): only for things that are literally typed
  or stored: paths, command lines, file names, keycaps.
- Scale: tokens `--t-mega` to `--t-xs` (tokens.css). Body 17 to 18 px, leading 1.55.

### Grid and rhythm

12 columns, gutter `clamp(16px, 2vw, 40px)`, outer margin = gutter. Three hang lines: col 1,
col 5, col 8. Section moods: hero full viewport; statement airy; picker normal; mechanism
sticky (tall); facts tight; index normal; finale full viewport.

### Shape

One radius: 10 px on every surface (window, pane, card, button, image); keycaps 6 px because
they are smaller objects. Nothing is pill-shaped.

### Motion language

- Eases: engine `ease.out` (expo.out) for entrances, `ease.inOut` for pane splits and the page
  transition, `ease.soft` for hovers.
- Open beside: blocks reveal with `clip-path: inset(0 0 0 100%)` to `inset(0)` plus `x: 40 → 0`
  (data-reveal="beside"), panes split by flex weight tweens. Text lines rise in masks (engine).
- Page transition: the next page opens as a window from the right edge in its page colour, with
  a title bar naming it and a green dot; the old page slides 6% left underneath. It then slides
  off to the left. 0.8 s each way.
- No loader: a product page that opens in 75 ms should not make people wait. The hero split is
  the intro.
- Idle: panes breathe (each pane's weight drifts by a few percent on the GSAP ticker, the open
  dots pulse once every few seconds, a caret blinks in each pane's input line), paused off screen,
  off under reduced motion.

### Micro-interactions (one per role)

| Role | Treatment |
|---|---|
| Download buttons | a fill opens from the right edge (a window opening beside), the ↓ arrow travels down |
| Nav links | the green open dot slides in beside the hovered link; the current page keeps its dot |
| Feature rows (index) | the row opens beside: a raised panel wipes in from the right, the title slides right 12 px, a real screenshot follows the pointer |
| Hero panes | the pane comes forward (widens), its name widens on the width axis |
| Inline links | underline at rest, rising into a band |
| Utility links (footer legal) | colour only |

### Composition plan

```
HOME
1 Hero (split frame)         ┌ header: wordmark · How it works · Guide · RU · Download ┐
                             │ [1 Main ●][2 Work ●][3 Personal ●][ + ]                  │
                             │   ALL YOUR CLAUDE ACCOUNTS, SIGNED IN AT ONCE.           │
                             └ sub copy (col 1-5)        Download x64 / ARM64 · Ctrl Alt C ┘
2 Statement                  large paragraph cols 1-10, muted second half
3 Picker (asymmetric pair)   real screenshot cols 1-7 | keycaps + key table cols 9-12
4 Mechanism (sticky)         sticky figure cols 1-7 (switcher → 3 windows → 3 folders) |
                             4 chapters cols 9-12; figure state follows the chapter
5 Facts (data)               4 numerals in a row: 75 ms, 16 MB, 550 KB, 0 requests; credit
6 Index list                 9 feature rows, full width, cursor preview of real screenshots
7 Finale (footer)            "Download. Run it. Click Add." mega, 3 steps, buttons, links, legal

HOW IT WORKS                 poster: "Each account is a folder." + path in mono
                             steps (numbered, real sequence) · separate vs shared (two columns)
                             details · usage sources table · security · comparison table
GUIDE                        sidebar index (sticky) + content columns, keycap tables
404                          one dashed "Add" pane: "There is no such account." + way home
```

### Explainer figure

The mechanism section is an explainer. It is built as a flat DOM/SVG figure (windows, folders,
the command line) rather than with the `anatomy` skill's isometric style: the product lives in
flat Windows 11 surfaces, and an isometric object would be a second visual language.

### Genericness test

- A generic model would put a centered hero with a screenshot in a glowing frame. Changed: the
  hero is the product's idea (windows side by side), the screenshot comes later at full size.
- A generic model would use three equal feature cards. Changed: features are an index list
  with real screenshots following the pointer.
- A generic dark tool site would add a neon accent. Changed: the only accent is the app's
  "Open" green, used only for the open state; colour otherwise belongs to accounts.
- "Fade up on every section". Changed: blocks open from the right edge, the verb of the site.

### Still or alive

Alive at rest in the hero only (pane drift, caret, dot pulse). Other sections are still at rest
by decision: their life is in hovers and the sticky figure.

## QA log

Production build, `shoot.mjs --crawl --nav` + `audit.mjs`, three fresh critic subagents (screenshots only).

| Pass | Critic overall | Main findings acted on |
|---|---|---|
| 1 | 7.29 | glyphs lost in window gaps, see-through header, preview over copy, text CTAs underline only, dead left column, RU "Menu" |
| 2 | 6.8 | regressions: sticky h2 over text on phones, headline copies wrapping differently (no `text-wrap: balance` on the copies), empty transition frame |
| 3 | 7.1 | mobile sticky figure, primary link without the CTA treatment, 404 slot not a link, TOC end state, code wrapping |

Final automated state: 0 audit FAIL/WARN, 0 axe violations, 0 overflow, 0 small targets,
60 fps, throttled mobile LCP 1.3 to 1.9 s, CLS 0, hero idle motion on.

What would raise the score most: carry the window motif into the bodies of How it works and
the Guide (each section as a window, the folder tree as the sticky left column), and make the
transition's settled frame show the incoming window's title bar.
