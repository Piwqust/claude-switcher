// Claude Profile Switcher — the scroll and motion layer. Needs GSAP, ScrollTrigger and Lenis from the CDN;
// without them (or with reduced motion) the page stays on its static versions, which are complete.
// One clock: GSAP's ticker drives Lenis, ScrollTrigger, the race and the pointer drift.

(() => {
  const { gsap, ScrollTrigger, Lenis } = window;
  if (!gsap || !ScrollTrigger) return;
  if (matchMedia('(prefers-reduced-motion: reduce)').matches) return;

  gsap.registerPlugin(ScrollTrigger);
  const root = document.documentElement;
  root.classList.add('motion');
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const fine = matchMedia('(hover: hover) and (pointer: fine)').matches;

  // ---------- smooth scroll on the shared clock ----------
  let lenis = null;
  if (Lenis) {
    lenis = new Lenis({ lerp: 0.11, smoothWheel: true });
    root.classList.add('lenis');
    lenis.on('scroll', ScrollTrigger.update);
    gsap.ticker.add((t) => lenis.raf(t * 1000));
    gsap.ticker.lagSmoothing(0);
    // in-page links glide instead of jumping, and land below the title bar
    document.addEventListener('click', (e) => {
      const a = e.target.closest && e.target.closest('a[href^="#"]');
      if (!a) return;
      const id = a.getAttribute('href');
      const target = id.length > 1 && $(id);
      if (!target) return;
      e.preventDefault();
      lenis.scrollTo(target, { offset: id === '#top' ? -200 : -64, duration: 1.2 });
      history.replaceState(null, '', id);
    });
  }

  // ---------- hero: the desktop tilts up into place, its windows fan out ----------
  const frame = $('[data-desk-frame]');
  if (frame) {
    const range = { trigger: '.desk-stage', start: 'top bottom', end: 'top 18%', scrub: 0.6 };
    gsap.fromTo(frame, { rotationX: 26, scale: 0.9, y: 40 }, { rotationX: 0, scale: 1, y: 0, ease: 'none', scrollTrigger: range });
    const fan = { Main: { xPercent: 42, rotation: -2.5 }, Work: { xPercent: -42, rotation: 2.5 }, Personal: { yPercent: 18, rotation: 0 } };
    $$('.cwin').forEach((w) => {
      const from = fan[w.dataset.win];
      if (from) gsap.fromTo(w, { ...from, yPercent: from.yPercent || 6 }, { xPercent: 0, yPercent: 0, rotation: 0, ease: 'none', scrollTrigger: range });
    });

    // idle: the wallpaper leans toward the pointer and settles back
    const paper = $('.wallpaper');
    if (paper && fine) {
      const qx = gsap.quickTo(paper, 'x', { duration: 1.4, ease: 'power3.out' });
      const qy = gsap.quickTo(paper, 'y', { duration: 1.4, ease: 'power3.out' });
      gsap.set(paper, { scale: 1.06 });
      addEventListener('pointermove', (e) => { qx((e.clientX / innerWidth - 0.5) * -28); qy((e.clientY / innerHeight - 0.5) * -18); }, { passive: true });
    }
  }

  // ---------- signature: one shortcut, played by the scroll ----------
  const story = $('[data-story]');
  if (story) {
    const steps = $$('.story__steps li', story);
    const key = (k) => $(`[data-key="${k}"]`, story);
    const picker = $('[data-story-picker]', story);
    const win = $('[data-story-win]', story);
    const pick = $('[data-story-pick]', story);
    const pickNum = pick && $('em', pick);
    const fill = $('[data-story-fill]', story);

    gsap.set(steps.slice(1), { autoAlpha: 0, yPercent: 70 });
    gsap.set([picker, win], { autoAlpha: 0, scale: 0.94, y: 18 });
    gsap.set(fill, { scaleX: 0 });

    const tl = gsap.timeline({
      defaults: { ease: 'power3.out', duration: 0.6 },
      scrollTrigger: { trigger: story, start: 'top top', end: 'bottom bottom', scrub: 0.8, invalidateOnRefresh: true },
    });
    const swap = (i, at) => {
      tl.to(steps[i - 1], { autoAlpha: 0, yPercent: -70, duration: 0.5, ease: 'power2.in' }, at)
        .to(steps[i], { autoAlpha: 1, yPercent: 0 }, at + 0.35);
    };
    const press = (k, at) => tl.to(key(k), { '--press': 1, duration: 0.35, ease: 'power2.out' }, at);
    const release = (k, at) => tl.to(key(k), { '--press': 0, duration: 0.35, ease: 'power2.inOut' }, at);

    tl.to(fill, { scaleX: 1, ease: 'none', duration: 10 }, 0);
    swap(1, 1.4); press('ctrl', 1.8); press('alt', 2.3);
    swap(2, 3.8); press('c', 4.2);
    tl.to(picker, { autoAlpha: 1, scale: 1, y: 0, duration: 0.7, ease: 'expo.out' }, 4.45);
    swap(3, 5.9); press('2', 6.3);
    tl.to(pick, { borderColor: '#e08566', boxShadow: '0 0 0 1px #e08566 inset', duration: 0.25 }, 6.45)
      .to(pickNum, { backgroundColor: '#e08566', color: '#1a0f0a', duration: 0.25 }, 6.45);
    swap(4, 7.8);
    ['ctrl', 'alt', 'c', '2'].forEach((k, i) => release(k, 7.9 + i * 0.05));
    tl.to(picker, { autoAlpha: 0, scale: 0.96, duration: 0.35, ease: 'power2.in' }, 7.95)
      .to(win, { autoAlpha: 1, scale: 1, y: 0, duration: 0.8, ease: 'expo.out' }, 8.3)
      .to({}, { duration: 1 }, 9);
  }

  // ---------- the names marquee speeds up and leans with the scroll ----------
  const track = $('.marquee__track');
  if (track && lenis) {
    const anim = track.getAnimations()[0];
    const skew = gsap.quickTo(track, 'skewX', { duration: 0.5, ease: 'power3.out' });
    let rate = 1;
    lenis.on('scroll', ({ velocity }) => {
      skew(gsap.utils.clamp(-8, 8, velocity * -0.35));
      rate = 1 + Math.min(6, Math.abs(velocity) * 0.25);
    });
    gsap.ticker.add(() => {
      if (!anim) return;
      anim.playbackRate += (rate - anim.playbackRate) * 0.08;
      rate += (1 - rate) * 0.06;
    });
  }

  // ---------- the race: both switchers start at once, in real time ----------
  const race = $('[data-race]');
  if (race) {
    const usBar = $('[data-race-us]', race), themBar = $('[data-race-them]', race);
    const usT = $('[data-race-us-t]', race), themT = $('[data-race-them-t]', race);
    const go = $('[data-race-go]', race);
    let tweens = [];
    const run = () => {
      tweens.forEach((t) => t.kill());
      gsap.set([usBar, themBar], { scaleX: 0 });
      usT.textContent = '0 ms'; themT.textContent = '0.0 s';
      const us = { v: 0 }, them = { v: 0 };
      tweens = [
        gsap.to(usBar, { scaleX: 1, duration: 0.075, ease: 'none' }),
        gsap.to(us, { v: 75, duration: 0.075, ease: 'none', onUpdate: () => (usT.textContent = `${Math.round(us.v)} ms`), onComplete: () => (usT.textContent = '75 ms') }),
        gsap.to(themBar, { scaleX: 1, duration: 10, ease: 'none' }),
        gsap.to(them, { v: 10, duration: 10, ease: 'none', onUpdate: () => (themT.textContent = `${them.v.toFixed(1)} s`), onComplete: () => (themT.textContent = 'about 10 s') }),
      ];
    };
    ScrollTrigger.create({ trigger: race, start: 'top 80%', once: true, onEnter: run });
    go && go.addEventListener('click', run);
  }

  // ---------- section headings rise line by line (on top of the CSS window-open reveal) ----------
  $$('.h2').forEach((h) => {
    gsap.from(h, { yPercent: 30, autoAlpha: 0, duration: 1.1, ease: 'expo.out', scrollTrigger: { trigger: h, start: 'top 88%', once: true } });
  });

  // layout settles once the fonts and images are in
  const refresh = () => ScrollTrigger.refresh();
  document.fonts && document.fonts.ready.then(refresh);
  addEventListener('load', refresh);
})();
