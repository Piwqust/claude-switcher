// The driver: one set of weights decides how wide each account window is. Everything that
// carries the concept reads it: each pane's clip, the offset of its content, where its status
// sits, and its name's width on Roboto Flex's wdth axis. Inputs: the intro (windows opening
// beside each other), the pointer or a number key (a window comes forward), and an idle drift
// (the windows are live). Per frame only clip-path, transform and the font's width axis change.
import { gsap } from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';
import { SplitText } from 'gsap/SplitText';
import { ease, isTouch, onPage, prefersReducedMotion } from '../engine';

gsap.registerPlugin(SplitText);

const GAP = 10;
const RADIUS = 10;
const ADD = 0.34; // the Add window is narrower
const FORWARD = 1.85;

type Pane = {
  el: HTMLElement;
  inner: HTMLElement;
  name: HTMLElement | null;
  status: HTMLElement | null;
  center: HTMLElement | null;
  natural: number; // name width at the font's default width
  statusW: number;
  centerW: number;
  w: number; // current weight
  open: number; // 0 → 1, the intro opens windows one by one
  lastStretch: number;
};

export function startPanes() {
  onPage(({ intro }) => {
    const root = document.querySelector<HTMLElement>('[data-panes]');
    if (!root || prefersReducedMotion()) return;
    const els = [...root.querySelectorAll<HTMLElement>('[data-pane]')];
    const n = els.length;
    const panes: Pane[] = els.map((el, i) => ({
      el,
      inner: el.querySelector<HTMLElement>('[data-pane-in]')!,
      name: el.querySelector<HTMLElement>('[data-pane-name]'),
      status: el.querySelector<HTMLElement>('.pane__status'),
      center: el.querySelector<HTMLElement>('.pane__center'),
      natural: 1,
      statusW: 0,
      centerW: 0,
      w: i === n - 1 ? ADD : 1,
      open: i === 0 ? 1 : 0,
      lastStretch: -1,
    }));
    let focus = -1; // which window is forward, -1 = none
    let keyTimer: gsap.core.Tween | null = null;
    let W = 0;
    let pad = 16;
    const touch = isTouch();

    const measure = () => {
      W = root.clientWidth;
      pad = parseFloat(getComputedStyle(panes[0].inner).paddingLeft) || 16;
      for (const p of panes) {
        if (p.name) {
          p.name.style.fontVariationSettings = '';
          p.natural = p.name.scrollWidth || 1;
          p.lastStretch = -1;
        }
        if (p.status) p.statusW = p.status.offsetWidth;
        if (p.center) p.centerW = p.center.offsetWidth;
      }
    };
    root.classList.add('is-live');

    // The headline runs across the windows. A dark copy lives inside each window and is
    // clipped with it; the real h1 sits behind them in ink, so letters stay readable in the gaps.
    const title = root.parentElement?.querySelector<HTMLElement>('.hero__title') ?? null;
    const copies: HTMLElement[] = [];
    const copyTweens: gsap.core.Tween[] = [];
    let copiesStarted = false;
    if (title) {
      for (const p of panes) {
        const c = document.createElement('p');
        c.className = 'hero__title hero__title--copy';
        c.setAttribute('aria-hidden', 'true');
        c.textContent = title.textContent;
        p.el.appendChild(c);
        copies.push(c);
        let done = false;
        SplitText.create(c, {
          type: 'lines,words', mask: 'lines', autoSplit: true, linesClass: 'split-line',
          onSplit(self) {
            if (done) return;
            const tw = gsap.from(self.words, {
              yPercent: 110, rotate: 2, duration: 1.1, stagger: 0.035, ease: ease.out, delay: 0.35,
              paused: !copiesStarted, onComplete: () => { done = true; },
            });
            copyTweens.push(tw);
            return tw;
          },
        });
      }
      intro.add(() => { copiesStarted = true; copyTweens.forEach((t) => t.play()); }, 0);
    }
    const placeCopies = () => {
      if (!title) return;
      const r = root.getBoundingClientRect();
      const t = title.getBoundingClientRect();
      for (const c of copies) {
        c.style.left = `${(t.left - r.left).toFixed(1)}px`;
        c.style.top = `${(t.top - r.top).toFixed(1)}px`;
        c.style.width = `${t.width.toFixed(1)}px`;
      }
    };

    measure();
    placeCopies();

    const dots = [...root.querySelectorAll<HTMLElement>('[data-dot]')];
    gsap.set(dots, { scale: 0 });

    const target = (i: number, t: number) => {
      const isAdd = i === n - 1;
      let w = isAdd ? ADD : 1;
      if (focus === i) w = isAdd ? 0.9 : FORWARD;
      return w * (1 + Math.sin(t * 0.42 + i * 2.1) * 0.05); // idle: a slow breath, out of phase
    };

    const tick = (t: number) => {
      const k = 1 - Math.pow(0.9, gsap.ticker.deltaRatio());
      let total = 0;
      let gaps = 0;
      for (let i = 0; i < n; i++) {
        const p = panes[i];
        p.w += (target(i, t) - p.w) * k;
        total += p.w * p.open;
        if (i > 0) gaps += GAP * Math.min(1, p.open * 4);
      }
      const avail = W - gaps;
      let x = 0;
      panes.forEach((p, i) => {
        if (i > 0) x += GAP * Math.min(1, p.open * 4);
        const width = total > 0 ? (avail * p.w * p.open) / total : 0;
        const right = Math.max(0, W - x - width);
        p.el.style.clipPath = `inset(0px ${right.toFixed(1)}px 0px ${x.toFixed(1)}px round ${RADIUS}px)`;
        p.inner.style.transform = `translate3d(${x.toFixed(1)}px,0,0)`;
        if (p.status) p.status.style.transform = `translate3d(${Math.max(0, width - p.statusW - pad * 2).toFixed(1)}px,0,0)`;
        if (p.center) {
          p.center.style.transform = `translate3d(${((width - p.centerW) / 2).toFixed(1)}px,0,0)`;
          p.center.style.opacity = String(gsap.utils.clamp(0, 1, (width - 48) / 36)); // never a clipped sliver
        }
        if (p.name) {
          // The name fills its window: condensed when narrow, wide when it comes forward.
          // Roboto Flex gets its real range from wdth and XTRA together (0.6x to 1.45x).
          const ratio = ((width - pad * 2) / p.natural) * 0.92;
          const s = Math.round(gsap.utils.clamp(0, 1, (ratio - 0.61) / 0.85) * 50) / 50; // coarse steps: fewer text relayouts
          if (s !== p.lastStretch) {
            p.name.style.fontVariationSettings = `'wdth' ${(25 + s * 126).toFixed(1)}, 'XTRA' ${(323 + s * 280).toFixed(0)}`;
            p.lastStretch = s;
          }
        }
        x += width;
      });
    };

    // Run only while the hero is on screen.
    let running = false;
    const run = (on: boolean) => {
      if (on === running) return;
      running = on;
      if (on) gsap.ticker.add(tick);
      else gsap.ticker.remove(tick);
    };
    run(true);
    const st = ScrollTrigger.create({ trigger: root, start: 'top bottom', end: 'bottom top', onToggle: (s) => run(s.isActive) });

    // Intro: the other windows open beside the first one, then their dots turn green.
    const opens = gsap.timeline({ paused: true });
    panes.slice(1).forEach((p, i) => opens.to(p, { open: 1, duration: 1.1, ease: ease.inOut }, 0.1 + i * 0.2));
    dots.forEach((d, i) => opens.to(d, { scale: 1, duration: 0.5, ease: ease.spring }, 0.5 + i * 0.2));
    intro.add(() => { opens.play(); }, 0);

    const setFocus = (i: number) => { focus = i; };
    const offs: (() => void)[] = [];
    panes.forEach((p, i) => {
      if (touch) {
        const down = () => setFocus(focus === i ? -1 : i);
        p.el.addEventListener('pointerdown', down);
        offs.push(() => p.el.removeEventListener('pointerdown', down));
      } else {
        const enter = () => setFocus(i);
        p.el.addEventListener('pointerenter', enter);
        offs.push(() => p.el.removeEventListener('pointerenter', enter));
      }
    });
    if (!touch) {
      const leave = () => setFocus(-1);
      root.addEventListener('pointerleave', leave);
      offs.push(() => root.removeEventListener('pointerleave', leave));
    }

    // Number keys bring a window forward, as they open an account in the picker.
    const key = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey || !running) return;
      const d = Number(e.key);
      if (!Number.isInteger(d) || d < 1 || d > n - 1) return;
      setFocus(d - 1);
      keyTimer?.kill();
      keyTimer = gsap.delayedCall(2.6, () => setFocus(-1));
    };
    addEventListener('keydown', key);
    offs.push(() => removeEventListener('keydown', key));

    const resize = () => { measure(); placeCopies(); };
    addEventListener('resize', resize);
    document.fonts.ready.then(resize).catch(() => {});
    offs.push(() => removeEventListener('resize', resize));

    return () => {
      run(false);
      st.kill();
      keyTimer?.kill();
      opens.kill();
      offs.forEach((f) => f());
      copies.forEach((c) => c.remove());
    };
  });
}
