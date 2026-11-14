// Global motion for this site: runs once on the client, before the first page initializes.
import { gsap } from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';
import { ease, isMobile, isTouch, onPage, onScroll, prefersReducedMotion, setTransition, startScroll, stopScroll } from '../engine';
import { startPanes } from './panes';

gsap.registerPlugin(ScrollTrigger);

// Where a navigation is going: the transition window takes that page's colour and name.
const pages: { test: RegExp; color: string; en: string; ru: string; enTitle: string; ruTitle: string }[] = [
  { test: /how-it-works/, color: '#5596c4', en: 'How it works', ru: 'Как это работает', enTitle: 'Each account is a folder.', ruTitle: 'Каждый аккаунт это папка.' },
  { test: /guide/, color: '#5fa872', en: 'Guide', ru: 'Руководство', enTitle: 'User guide', ruTitle: 'Руководство' },
  { test: /^\/(ru\/)?$/, color: '#e08a6b', en: 'Claude Profile Switcher', ru: 'Claude Profile Switcher', enTitle: 'All open at once.', ruTitle: 'Все открыты сразу.' },
];
let dest = { color: '#3a3a3a', label: '', title: '' };

function pageTransition() {
  document.addEventListener('astro:before-preparation', (e) => {
    const to = e.to;
    const ru = to.pathname.startsWith('/ru/');
    const hit = pages.find((p) => p.test.test(to.pathname));
    dest = { color: hit?.color ?? '#3a3a3a', label: hit ? (ru ? hit.ru : hit.en) : '', title: hit ? (ru ? hit.ruTitle : hit.enTitle) : '' };
    document.querySelector<HTMLElement>('[data-menu-btn][aria-expanded="true"]')?.click();
  });

  // Open beside: the next page arrives as a window from the right edge while the current one
  // slides back a little; then that window moves on to the left and the new page is there.
  setTransition({
    leave: (c) => {
      const panel = c.querySelector<HTMLElement>('.curtain__panel')!;
      const label = c.querySelector<HTMLElement>('[data-curtain-label]');
      const title = c.querySelector<HTMLElement>('[data-curtain-title]');
      if (label) label.textContent = dest.label;
      if (title) title.textContent = dest.title;
      panel.style.setProperty('--pane', dest.color);
      return gsap.timeline()
        .set(c, { autoAlpha: 1 })
        .fromTo(panel, { xPercent: 102 }, { xPercent: 0, duration: 0.8, ease: ease.inOut }, 0)
        .fromTo(c.querySelector('.curtain__bar'), { x: 60 }, { x: 0, duration: 0.8, ease: ease.inOut }, 0)
        .fromTo(title, { x: 160 }, { x: 0, duration: 0.8, ease: ease.inOut }, 0)
        .to('#main', { xPercent: -6, duration: 0.8, ease: ease.inOut }, 0);
    },
    // The window then settles into the new page's own window (its hero) and hands over.
    enter: (c) => {
      const panel = c.querySelector<HTMLElement>('.curtain__panel')!;
      const words = c.querySelectorAll('.curtain__bar, [data-curtain-title]');
      const target = document.querySelector<HTMLElement>('.pagewin, [data-panes]');
      const tl = gsap.timeline();
      if (!target) {
        return tl.to(panel, { xPercent: -102, duration: 0.85, ease: ease.inOut }).set(c, { autoAlpha: 0 });
      }
      const r = target.getBoundingClientRect();
      const inset = `inset(${r.top}px ${innerWidth - r.right}px ${innerHeight - r.bottom}px ${r.left}px round 10px)`;
      return tl
        .to(words, { autoAlpha: 0, duration: 0.25, ease: ease.soft }, 0)
        .fromTo(panel, { clipPath: 'inset(0px 0px 0px 0px round 0px)' }, { clipPath: inset, duration: 0.8, ease: ease.inOut }, 0)
        .set(c, { autoAlpha: 0 })
        .set(panel, { clearProps: 'clipPath' })
        .set(words, { clearProps: 'opacity,visibility' });
    },
  });
}

// Download buttons: the fill opens from the right edge (CSS covers touch and reduced motion).
function buttonFill() {
  onPage(() => {
    if (isTouch() || prefersReducedMotion()) return;
    const off: (() => void)[] = [];
    document.querySelectorAll<HTMLElement>('[data-fill]').forEach((btn) => {
      const fill = btn.querySelector<HTMLElement>('.btn__fill');
      if (!fill) return;
      const open = () => {
        btn.classList.add('is-filled');
        gsap.to(fill, { clipPath: 'inset(0% 0% 0% 0% round 0px)', duration: 0.55, ease: ease.soft, overwrite: true });
      };
      const close = () => {
        btn.classList.remove('is-filled');
        gsap.to(fill, { clipPath: 'inset(0% 0% 0% 100% round 0px)', duration: 0.45, ease: ease.soft, overwrite: true });
      };
      const focus = () => btn.matches(':focus-visible') && open();
      btn.addEventListener('pointerenter', open); btn.addEventListener('pointerleave', close);
      btn.addEventListener('focus', focus); btn.addEventListener('blur', close);
      off.push(() => {
        btn.removeEventListener('pointerenter', open); btn.removeEventListener('pointerleave', close);
        btn.removeEventListener('focus', focus); btn.removeEventListener('blur', close);
      });
    });
    return () => off.forEach((f) => f());
  });
}

// Phone menu: a full-screen panel. Focus stays inside, Escape closes, focus returns.
function menu() {
  onPage(() => {
    const btn = document.querySelector<HTMLButtonElement>('[data-menu-btn]');
    const nav = document.querySelector<HTMLElement>('[data-menu]');
    if (!btn || !nav) return;
    const mq = matchMedia('(max-width: 767px)');
    const items = () => [btn, ...nav.querySelectorAll<HTMLElement>('a')];
    const set = (open: boolean) => {
      btn.setAttribute('aria-expanded', String(open));
      document.documentElement.classList.toggle('menu-open', open);
      if (open) {
        stopScroll();
        if (!prefersReducedMotion()) {
          gsap.fromTo(nav, { clipPath: 'inset(0% 0% 0% 100%)' }, { clipPath: 'inset(0% 0% 0% 0%)', duration: 0.7, ease: ease.inOut });
          gsap.fromTo(nav.querySelectorAll('li, .hdr__dl'), { x: 60, autoAlpha: 0 }, { x: 0, autoAlpha: 1, duration: 0.8, ease: ease.out, stagger: 0.05, delay: 0.15 });
        }
        nav.querySelector<HTMLElement>('a')?.focus();
      } else {
        startScroll();
        gsap.set(nav, { clearProps: 'clipPath' });
        gsap.set(nav.querySelectorAll('li, .hdr__dl'), { clearProps: 'all' });
      }
    };
    const toggle = () => set(btn.getAttribute('aria-expanded') !== 'true');
    const key = (e: KeyboardEvent) => {
      if (btn.getAttribute('aria-expanded') !== 'true') return;
      if (e.key === 'Escape') { set(false); btn.focus(); return; }
      if (e.key !== 'Tab') return;
      const list = items();
      const i = list.indexOf(document.activeElement as HTMLElement);
      const next = e.shiftKey ? (i <= 0 ? list.length - 1 : i - 1) : (i === list.length - 1 ? 0 : i + 1);
      e.preventDefault();
      list[next].focus();
    };
    const change = () => { if (!mq.matches) set(false); };
    btn.addEventListener('click', toggle);
    document.addEventListener('keydown', key);
    mq.addEventListener('change', change);
    return () => {
      btn.removeEventListener('click', toggle);
      document.removeEventListener('keydown', key);
      mq.removeEventListener('change', change);
      document.documentElement.classList.remove('menu-open');
    };
  });
}

// The header steps up out of the way while reading down, and comes back on the way up.
function headerAway() {
  let last = 0;
  onScroll(({ y }) => {
    if (Math.abs(y - last) < 4) return;
    document.documentElement.classList.toggle('hdr-away', y > 200 && y > last);
    document.documentElement.classList.toggle('hdr-solid', y > 40);
    last = y;
  });
}

// Keycaps answer the real keys. Digits light the 1…9 cap.
function keycaps() {
  onPage(() => {
    if (!document.querySelector('[data-key]')) return;
    const id = (e: KeyboardEvent) => (/^[1-9]$/.test(e.key) ? 'digit' : e.key === 'Control' ? 'ctrl' : e.key.toLowerCase());
    const light = (e: KeyboardEvent, on: boolean) => {
      document.querySelectorAll<HTMLElement>(`[data-key="${CSS.escape(id(e))}"]`).forEach((k) => k.classList.toggle('is-down', on));
    };
    const down = (e: KeyboardEvent) => light(e, true);
    const up = (e: KeyboardEvent) => light(e, false);
    const clear = () => document.querySelectorAll('.key.is-down').forEach((k) => k.classList.remove('is-down'));
    addEventListener('keydown', down); addEventListener('keyup', up); addEventListener('blur', clear);
    return () => { removeEventListener('keydown', down); removeEventListener('keyup', up); removeEventListener('blur', clear); };
  });
}

// Mechanism: the figure follows the chapter being read.
function mechanism() {
  onPage(() => {
    const stage = document.querySelector<HTMLElement>('[data-stage]');
    const chapters = gsap.utils.toArray<HTMLElement>('[data-chapter]');
    if (!stage || !chapters.length) return;
    if (prefersReducedMotion() || isMobile()) { stage.dataset.state = '3'; chapters.forEach((c) => c.classList.add('is-active')); return; }
    const setState = (i: number) => {
      stage.dataset.state = String(i);
      chapters.forEach((c, j) => c.classList.toggle('is-active', j === i));
    };
    setState(0);
    chapters.forEach((ch, i) => ScrollTrigger.create({
      trigger: ch, start: 'top 62%', end: 'bottom 62%',
      onToggle: (self) => { if (self.isActive) setState(i); },
    }));
  });
}

// Real numbers count up once.
function counters() {
  onPage(() => {
    if (prefersReducedMotion()) return;
    document.querySelectorAll<HTMLElement>('[data-count]').forEach((el) => {
      const end = Number(el.dataset.count);
      if (!end) return;
      const o = { v: 0 };
      gsap.to(o, {
        v: end, duration: 1.6, ease: ease.out,
        scrollTrigger: { trigger: el, start: 'top 88%', once: true },
        onUpdate: () => { el.textContent = String(Math.round(o.v)); },
      });
    });
  });
}

// Feature index: a real screenshot follows the pointer (fine pointers only).
function indexPreview() {
  onPage(() => {
    const list = document.querySelector<HTMLElement>('[data-index]');
    const box = document.querySelector<HTMLElement>('[data-preview-box]');
    if (!list || !box || isTouch() || prefersReducedMotion()) return;
    const img = box.querySelector('img')!;
    list.querySelectorAll<HTMLElement>('[data-preview]').forEach((a) => { const i = new Image(); i.src = a.dataset.preview!; });
    const x = gsap.quickTo(box, 'x', { duration: 0.7, ease: 'power3.out' });
    const y = gsap.quickTo(box, 'y', { duration: 0.7, ease: 'power3.out' });
    let px = 0, py = 0, shown = false;
    const move = (e: PointerEvent) => {
      px = e.clientX; py = e.clientY;
      const d = (e.target as HTMLElement).closest('a')?.querySelector<HTMLElement>('.index__d');
      const band = d ? d.getBoundingClientRect().right + 28 : innerWidth - box.offsetWidth - 120; // clear of the text
      x(Math.min(band, innerWidth - box.offsetWidth - 16) + (px / innerWidth - 0.5) * 16);
      y(py);
    };
    const show = (e: Event) => {
      const a = e.currentTarget as HTMLElement;
      const src = a.dataset.preview;
      if (!src) return hide();
      if (img.getAttribute('src') !== src) img.src = src;
      if (!shown) gsap.fromTo(box, { clipPath: 'inset(0% 0% 0% 100%)' }, { clipPath: 'inset(0% 0% 0% 0%)', autoAlpha: 1, duration: 0.6, ease: ease.inOut, overwrite: true });
      shown = true;
    };
    const hide = () => {
      if (!shown) return;
      shown = false;
      gsap.to(box, { clipPath: 'inset(0% 100% 0% 0%)', duration: 0.45, ease: ease.inOut, overwrite: true, onComplete: () => { gsap.set(box, { autoAlpha: 0 }); } });
    };
    const rows = [...list.querySelectorAll<HTMLElement>('a')];
    rows.forEach((a) => a.addEventListener('pointerenter', show));
    list.addEventListener('pointerleave', hide);
    addEventListener('pointermove', move, { passive: true });
    // pointerleave never fires when the page scrolls under a still pointer
    const off = onScroll(() => { if (shown && !list.contains(document.elementFromPoint(px, py))) hide(); });
    return () => {
      rows.forEach((a) => a.removeEventListener('pointerenter', show));
      list.removeEventListener('pointerleave', hide);
      removeEventListener('pointermove', move);
      off();
    };
  });
}

// Guide: the table of contents marks the section being read.
function tocCurrent() {
  onPage(() => {
    const links = gsap.utils.toArray<HTMLAnchorElement>('.toc-link');
    if (!links.length) return;
    links.forEach((a, i) => {
      const sec = document.querySelector<HTMLElement>(a.hash);
      if (!sec) return;
      const last = i === links.length - 1;
      ScrollTrigger.create({
        trigger: sec, start: 'top 35%', end: last ? 'max' : 'bottom 35%',
        onToggle: (self) => {
          a.classList.toggle('is-current', self.isActive);
          if (self.isActive) a.setAttribute('aria-current', 'location'); else a.removeAttribute('aria-current');
        },
      });
    });
  });
}

export function setupMotion() {
  tocCurrent();
  pageTransition();
  buttonFill();
  menu();
  headerAway();
  keycaps();
  mechanism();
  counters();
  indexPreview();
  startPanes();
}
