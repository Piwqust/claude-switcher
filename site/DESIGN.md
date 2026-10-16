# Site design notes

## Concept card

- **Idea:** the page is the app at work: a Windows desktop where one shortcut opens every Claude account.
- **Metaphor:** Windows 11 itself: windows that open, a taskbar that fills, Fluent light that follows the pointer.
- **Signature:** "One shortcut", a pinned scroll story. Huge keycaps press Ctrl, Alt, C, the picker opens, 2 picks Work, its window opens signed in. Scroll is the finger.
- **Emotion:** the first 3 seconds feel like your own PC, already set up.
- **Motion verb:** *open like a window*: scale 0.96 → 1 with a fade, decelerate ease `cubic-bezier(0, 0, 0, 1)`; keys *press* (down 6 px, light up).
- **Screenshot:** the three giant keys lit terracotta with the picker opening above them.

## Motion system

- One ease family: Fluent decelerate `cubic-bezier(0, 0, 0, 1)` for opening, `cubic-bezier(0.2, 0, 0, 1)` for UI state.
- One clock: GSAP's ticker drives Lenis, ScrollTrigger, the race timer and the idle drift. No scroll listeners, no own rAF.
- Hero intro: the headline rises word by word from a mask (CSS, works before scripts load), then the desktop tilts up into place as you scroll and the open windows fan out from behind the picker.
- Reveals: sections *open like windows* (scale + fade + a short rise), staggered 60–100 ms inside a group.
- Hover: primary buttons fill from the pointer's side; the title bar marker slides to the hovered link; cards, settings rows and expanders get Fluent "reveal" light at the pointer. Utility links change colour only.
- Idle: the wallpaper drifts; the desktop windows lean a few pixels toward the pointer.
- Reduced motion: no smooth scroll, no scrub, no pinning; the story becomes a static list next to its final frame; everything is visible.
- Without GSAP (CDN blocked): the CSS reveals and every interactive demo still work; the story is the static version.
