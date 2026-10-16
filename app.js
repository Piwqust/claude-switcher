// Claude Profile Switcher — site. No dependencies. Everything works without this file; it only makes
// the picker, the countdown control, the FAQ drawer and the download choice respond.

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
  const tryLine = $('[data-try]');
  const toggles = $$('[data-toggle-picker]');
  const cards = $$('.acct[data-acct]');
  const BADGE = { Work: ['W', '#3d77a6'], Personal: ['P', '#3f8a58'] };
  let front = 'Work';
  let picked = new Set();

  const say = (text) => { if (live) live.textContent = text; };

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

  function setStatus(card, text) { $('[data-status]', card).textContent = text; }

  function openAccount(card) {
    const name = card.dataset.name;
    if (card.classList.contains('is-open')) {
      front = name;
      renderTaskbar();
      say(`${name} is already open; its window comes forward`);
      return;
    }
    card.classList.add('is-launching');
    setStatus(card, 'Opening…');
    const done = () => {
      card.classList.remove('is-launching');
      card.classList.add('is-open');
      setStatus(card, 'Open');
      front = name;
      renderTaskbar(name);
      say(`${name} opened, signed in`);
    };
    reduced ? done() : setTimeout(done, 520);
  }

  function closeAccount(card) {
    const name = card.dataset.name;
    card.classList.remove('is-open', 'is-front');
    setStatus(card, 'Just now');
    if (front === name) {
      const other = cards.find((c) => c.classList.contains('is-open'));
      front = other ? other.dataset.name : '';
    }
    renderTaskbar();
    say(`${name} closed`);
  }

  function updatePicked() {
    cards.forEach((c) => {
      const on = picked.has(c);
      c.classList.toggle('is-picked', on);
      c.setAttribute('aria-pressed', String(on));
    });
    if (!many) return;
    if (picked.size >= 2) {
      many.hidden = false;
      many.textContent = `Open ${picked.size} accounts`;
      if (tip) tip.hidden = true;
    } else {
      many.hidden = true;
      if (tip) tip.hidden = false;
    }
  }

  function setPickerShown(show) {
    if (!picker) return;
    picker.classList.toggle('is-hidden', !show);
    toggles.forEach((t) => t.setAttribute('aria-expanded', String(show)));
    say(show ? 'Picker shown' : 'Picker hidden. Press Ctrl+Alt+C to bring it back');
    if (tryLine) tryLine.textContent = show ? 'Try it: press 1, 2 or 3, or Ctrl+Alt+C' : 'Press Ctrl+Alt+C, or click the C, to bring it back';
  }

  if (picker) {
    if (matchMedia('(pointer: coarse)').matches && tryLine) tryLine.textContent = 'Try it: tap a card. Close a window from the taskbar.';
    cards.forEach((card) => {
      card.addEventListener('click', (e) => {
        if (e.ctrlKey || e.metaKey) {
          picked.has(card) ? picked.delete(card) : picked.add(card);
          updatePicked();
          return;
        }
        picked.clear(); updatePicked();
        openAccount(card);
      });
    });
    many && many.addEventListener('click', () => {
      picked.forEach((c) => openAccount(c));
      picked.clear(); updatePicked();
    });
    toggles.forEach((t) => t.addEventListener('click', () => setPickerShown(picker.classList.contains('is-hidden'))));

    document.addEventListener('keydown', (e) => {
      const tag = (e.target.tagName || '').toLowerCase();
      if (tag === 'input' || tag === 'textarea') return;
      if (e.ctrlKey && e.altKey && (e.key === 'c' || e.key === 'C' || e.code === 'KeyC')) {
        e.preventDefault();
        setPickerShown(picker.classList.contains('is-hidden'));
        if (!picker.classList.contains('is-hidden')) picker.scrollIntoView({ block: 'nearest', behavior: reduced ? 'auto' : 'smooth' });
        return;
      }
      const n = /^Digit([1-9])$/.exec(e.code);
      if (!n) return;
      const card = cards[Number(n[1]) - 1];
      if (!card) return;
      const heroVisible = picker.getBoundingClientRect().bottom > 0 && picker.getBoundingClientRect().top < innerHeight;
      if (e.ctrlKey && e.altKey) { e.preventDefault(); openAccount(card); return; }
      if (e.ctrlKey || e.altKey || e.metaKey || picker.classList.contains('is-hidden') || !heroVisible) return;
      openAccount(card);
    });
    renderTaskbar();
  }

  // ---------- the film ----------
  const film = $('.film__video');
  const filmBtn = $('[data-film-toggle]');
  if (film && filmBtn) {
    if (reduced) { film.removeAttribute('autoplay'); film.pause(); filmBtn.textContent = 'Play'; filmBtn.setAttribute('aria-pressed', 'true'); }
    filmBtn.addEventListener('click', () => {
      if (film.paused) { film.play().catch(() => {}); filmBtn.textContent = 'Pause'; filmBtn.setAttribute('aria-pressed', 'false'); }
      else { film.pause(); filmBtn.textContent = 'Play'; filmBtn.setAttribute('aria-pressed', 'true'); }
    });
  }

  // ---------- the countdown control ----------
  const show = $('.show');
  const word = $('[data-show-word]');
  const seg = $$('.seg [role="radio"]');
  function pickSeg(btn, focus) {
    seg.forEach((b) => { const on = b === btn; b.setAttribute('aria-checked', String(on)); b.tabIndex = on ? 0 : -1; });
    const s = Number(btn.dataset.sec);
    show.dataset.show = s ? 'on' : 'off';
    word.textContent = s ? `in ${s} s` : 'when you choose';
    if (focus) btn.focus();
  }
  if (show && word && seg.length) {
    seg.forEach((b, i) => {
      b.tabIndex = b.getAttribute('aria-checked') === 'true' ? 0 : -1;
      b.addEventListener('click', () => pickSeg(b));
      b.addEventListener('keydown', (e) => {
        const d = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
        if (!d) return;
        e.preventDefault();
        pickSeg(seg[(i + d + seg.length) % seg.length], true);
      });
    });
  }

  // ---------- FAQ drawer: one card open at a time ----------
  const fcards = $$('.fcard');
  fcards.forEach((card) => {
    const btn = $('button', card);
    btn.addEventListener('click', () => {
      const opening = !card.classList.contains('is-open');
      fcards.forEach((c) => { c.classList.remove('is-open'); $('button', c).setAttribute('aria-expanded', 'false'); });
      if (opening) { card.classList.add('is-open'); btn.setAttribute('aria-expanded', 'true'); }
    });
  });

  // ---------- download: name the next account, choose the build ----------
  const form = $('[data-get]');
  if (form) {
    const field = $('#acct-name');
    const initial = $('[data-initial]');
    const colours = ['#3d77a6', '#3f8a58', '#8b5fb3', '#b0603f', '#2f7f86'];
    field.addEventListener('input', () => {
      const v = field.value.trim();
      initial.textContent = v ? v[0].toUpperCase() : '?';
      initial.style.background = colours[(v.charCodeAt(0) || 0) % colours.length];
    });
    form.addEventListener('submit', (e) => {
      e.preventDefault();
      const arch = (new FormData(form).get('arch') || 'x64');
      location.href = `https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-${arch}.exe`;
    });
  }

  // ---------- reveals: once, staggered within each group ----------
  const groups = [['.board .tile'], ['.steps .step'], ['.drawer .fcard'], ['.word .wk'], ['.stats [data-reveal]']];
  groups.forEach(([sel]) => $$(sel).forEach((el, i) => el.style.setProperty('--d', i)));
  const targets = $$('[data-reveal]');
  if (!('IntersectionObserver' in window)) { targets.forEach((el) => el.classList.add('is-in')); return; }
  const io = new IntersectionObserver((entries) => {
    for (const entry of entries) {
      if (!entry.isIntersecting) continue;
      entry.target.classList.add('is-in');
      io.unobserve(entry.target);
    }
  }, { rootMargin: '0px 0px -10% 0px', threshold: 0.08 });
  targets.forEach((el) => io.observe(el));
  addEventListener('load', () => setTimeout(() => {
    targets.forEach((el) => { const r = el.getBoundingClientRect(); if (r.top < innerHeight && r.bottom > 0) el.classList.add('is-in'); });
  }, 2500));
})();
