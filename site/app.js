// Claude Profile Switcher — site. No dependencies. The page reads fine without this file; it makes the
// hero picker work, plays the film and keeps the small interface demos moving.

(() => {
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ---------- the picker in the hero ----------
  const picker = $('#picker');
  const taskbar = $('[data-taskbar]');
  const live = $('[data-live]');
  const tip = $('[data-tip]');
  const many = $('[data-open-many]');
  const hint = $('[data-try]');
  const reopen = $('.hero__reopen');
  const cards = $$('.acct[data-acct]');
  const BADGE = { Work: ['W', '#3d77a6'], Personal: ['P', '#3f8a58'] };
  const picked = new Set();
  let front = 'Work';

  const say = (text) => { if (live) live.textContent = text; };
  const setStatus = (card, text) => { $('[data-status]', card).textContent = text; };

  function renderTaskbar(added) {
    if (!taskbar) return;
    taskbar.textContent = '';
    for (const card of cards) {
      if (!card.classList.contains('is-open')) continue;
      const name = card.dataset.name;
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'taskbar__app' + (name === front ? ' is-front' : '') + (name === added ? ' is-new' : '');
      b.title = `Claude, ${name}. Click to close this window`;
      b.setAttribute('aria-label', `Claude, ${name}. Close this window`);
      const img = document.createElement('img');
      img.src = 'img/main-icon.png'; img.width = 24; img.height = 24; img.alt = '';
      b.append(img);
      if (BADGE[name]) {
        const badge = document.createElement('b');
        badge.textContent = BADGE[name][0];
        badge.style.background = BADGE[name][1];
        b.append(badge);
      }
      b.addEventListener('click', () => closeAccount(card));
      taskbar.append(b);
    }
    cards.forEach((c) => c.classList.toggle('is-front', c.dataset.name === front && c.classList.contains('is-open')));
  }

  function openAccount(card) {
    const name = card.dataset.name;
    if (card.classList.contains('is-open')) {
      front = name; renderTaskbar();
      say(`${name} is already open; its window comes forward`);
      return;
    }
    card.classList.add('is-launching');
    setStatus(card, 'Opening…');
    const done = () => {
      card.classList.remove('is-launching');
      card.classList.add('is-open');
      setStatus(card, 'Open');
      front = name; renderTaskbar(name);
      say(`${name} opened, signed in`);
      if (hint) hint.textContent = 'Click a taskbar button to close it';
    };
    reduced ? done() : setTimeout(done, 520);
  }

  function closeAccount(card) {
    const name = card.dataset.name;
    card.classList.remove('is-open', 'is-front');
    setStatus(card, 'Just now');
    if (front === name) { const other = cards.find((c) => c.classList.contains('is-open')); front = other ? other.dataset.name : ''; }
    renderTaskbar();
    say(`${name} closed`);
  }

  function updatePicked() {
    cards.forEach((c) => { const on = picked.has(c); c.classList.toggle('is-picked', on); c.setAttribute('aria-pressed', String(on)); });
    if (!many) return;
    const show = picked.size >= 2;
    many.hidden = !show;
    if (show) many.textContent = `Open ${picked.size} accounts`;
    if (tip) tip.hidden = show;
  }

  function setPickerShown(show) {
    picker.classList.toggle('is-hidden', !show);
    if (reopen) reopen.hidden = show;
    say(show ? 'Picker shown' : 'Picker hidden. Press Ctrl+Alt+C to bring it back');
  }

  if (picker) {
    if (matchMedia('(pointer: coarse)').matches && hint) hint.textContent = 'Try it: tap a card';
    cards.forEach((card) => card.addEventListener('click', (e) => {
      if (e.ctrlKey || e.metaKey) { picked.has(card) ? picked.delete(card) : picked.add(card); updatePicked(); return; }
      picked.clear(); updatePicked();
      openAccount(card);
    }));
    many && many.addEventListener('click', () => { picked.forEach((c) => openAccount(c)); picked.clear(); updatePicked(); });
    $$('[data-toggle-picker]').forEach((t) => t.addEventListener('click', () => setPickerShown(picker.classList.contains('is-hidden'))));

    document.addEventListener('keydown', (e) => {
      const tag = (e.target.tagName || '').toLowerCase();
      if (tag === 'input' || tag === 'textarea') return;
      if (e.ctrlKey && e.altKey && e.code === 'KeyC') {
        e.preventDefault();
        setPickerShown(picker.classList.contains('is-hidden'));
        return;
      }
      const n = /^Digit([1-9])$/.exec(e.code);
      if (!n) return;
      const card = cards[Number(n[1]) - 1];
      if (!card) return;
      if (e.ctrlKey && e.altKey) { e.preventDefault(); openAccount(card); return; }
      const r = picker.getBoundingClientRect();
      const visible = r.bottom > 0 && r.top < innerHeight;
      if (e.ctrlKey || e.altKey || e.metaKey || picker.classList.contains('is-hidden') || !visible) return;
      openAccount(card);
    });
    renderTaskbar();
  }

  // ---------- the film ----------
  const film = $('.hero__film');
  const video = $('.hero__video');
  function playFilm() {
    if (!video) return;
    film.classList.add('is-playing');
    video.play().catch(() => film.classList.remove('is-playing'));
  }
  $('[data-film]') && $('[data-film]').addEventListener('click', playFilm);
  $('[data-film-cta]') && $('[data-film-cta]').addEventListener('click', () => {
    film.scrollIntoView({ behavior: reduced ? 'auto' : 'smooth', block: 'center' });
    playFilm();
  });
  video && video.addEventListener('click', () => {
    if (video.paused) playFilm(); else { video.pause(); film.classList.remove('is-playing'); }
  });

  // ---------- small demos that keep moving (only while on screen) ----------
  const onScreen = new Set();
  const watch = new IntersectionObserver((es) => es.forEach((en) => en.isIntersecting ? onScreen.add(en.target) : onScreen.delete(en.target)));
  const loop = (el, ms, fn) => { if (!el || reduced) return; watch.observe(el); setInterval(() => { if (onScreen.has(el) && !document.hidden) fn(); }, ms); };

  // launcher: the shortcut is pressed and the selection walks down the list
  const launcher = $('.launcher');
  const rows = $$('.launcher .lrow:not(.lrow--dim)');
  const capsL = $$('.launcher__keys .cap');
  let li = 0;
  loop(launcher, 1800, () => {
    capsL.forEach((c, i) => setTimeout(() => c.classList.add('is-down'), i * 90));
    setTimeout(() => capsL.forEach((c) => c.classList.remove('is-down')), 420);
    rows.forEach((r) => r.classList.remove('is-sel'));
    li = (li + 1) % rows.length;
    rows[li] && rows[li].classList.add('is-sel');
  });

  // countdown card: the number follows the bar
  const count = $('[data-count]');
  let n = 5;
  loop(count && count.closest('.card'), 1000, () => { n = n <= 1 ? 5 : n - 1; count.textContent = n; });

  // hotkeys card: Ctrl+Alt+1…9
  const cycle = $('[data-cycle]');
  const capsW = $$('.card--wide .cap');
  let d = 2;
  loop(cycle && cycle.closest('.card'), 1500, () => {
    d = d % 9 + 1; cycle.textContent = d;
    capsW.forEach((c) => c.classList.add('is-down'));
    setTimeout(() => capsW.forEach((c) => c.classList.remove('is-down')), 260);
  });

  // ---------- reveals: once, staggered within each row ----------
  [['.wall .card'], ['.glows .gcard'], ['.trio .tcard']].forEach(([sel]) => $$(sel).forEach((el, i) => el.style.setProperty('--d', i % 4)));
  const targets = $$('[data-reveal]');
  if (!('IntersectionObserver' in window)) { targets.forEach((el) => el.classList.add('is-in')); return; }
  const io = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (!entry.isIntersecting) continue;
      entry.target.classList.add('is-in');
      io.unobserve(entry.target);
    }
  }, { rootMargin: '0px 0px -8% 0px', threshold: 0.08 });
  targets.forEach((el) => io.observe(el));
  addEventListener('load', () => setTimeout(() => {
    targets.forEach((el) => { const r = el.getBoundingClientRect(); if (r.top < innerHeight && r.bottom > 0) el.classList.add('is-in'); });
  }, 2500));
})();
