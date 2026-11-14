# Website

The site for Claude Profile Switcher: Astro, GSAP and Lenis, English at `/` and Russian at `/ru/`.
The concept, palette, type and motion rules are in [DESIGN.md](DESIGN.md).

```sh
npm install
npm run dev        # local development
npm run build      # static site in dist/
npm run preview    # serve dist/
```

- All copy, in both languages, is in `src/data/site.ts`.
- Interface screenshots are in `src/assets/shots/<lang>/` and come from `docs/images` (the app's demo mode).
- The hero's account windows are driven by `src/motion/panes.ts`; every other motion is registered in `src/motion/setup.ts`.
- `public/og.png` and `public/og-ru.png` are renders of the hero at 1200x630.
