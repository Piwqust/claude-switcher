// Claude Profile Switcher — site. No dependencies. The page reads fine without this file; it makes the
// desktop in the hero, the settings window and the download choice respond like the app does.

(() => {
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
  const reduced = matchMedia('(prefers-reduced-motion: reduce)').matches;

  // ---------- the desktop: picker, Claude windows, taskbar ----------
  const picker = $('#picker');
  const taskbar = $('[data-taskbar]');
  const live = $('[data-live]');
  const tip = $('[data-tip]');
  const many = $('[data-open-many]');
  const hint = $('[data-try]');
  const cards = $$('.acct[data-acct]');
  const wins = Object.fromEntries($$('.cwin').map((w) => [w.dataset.win, w]));
  const BADGE = { Work: ['W', '#3d77a6'], Personal: ['P', '#3f8a58'] };
  const picked = new Set();
  let front = 'Work';
  const wide = matchMedia('(min-width: 681px)');

  const say = (text) => { if (live) live.textContent = text; };
  const card = (name) => cards.find((c) => c.dataset.name === name);
  const setStatus = (c, text) => { $('[data-status]', c).textContent = text; };

  function render(added) {
    for (const c of cards) {
      const name = c.dataset.name;
      const open = c.classList.contains('is-open');
      c.classList.toggle('is-front', open && name === front);
      const w = wins[name];
      if (w) { w.classList.toggle('is-open', open); w.classList.toggle('is-front', open && name === front); }
    }
    if (!taskbar) return;
    taskbar.textContent = '';
    for (const c of cards) {
      if (!c.classList.contains('is-open')) continue;
      const name = c.dataset.name;
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'tb tb--app' + (name === front ? ' is-front' : '') + (name === added ? ' is-new' : '');
      b.setAttribute('aria-label', `Claude, ${name}`);
      b.title = `Claude, ${name}`;
      const img = document.createElement('img');
      img.src = 'img/main-icon.png'; img.width = 24; img.height = 24; img.alt = '';
      b.append(img);
      if (BADGE[name]) {
        const badge = document.createElement('b');
        badge.textContent = BADGE[name][0];
        badge.style.background = BADGE[name][1];
        b.append(badge);
      }
      b.addEventListener('click', () => { front = name; render(); say(`${name} comes forward`); });
      taskbar.append(b);
    }
  }

  function openAccount(c) {
    const name = c.dataset.name;
    if (c.classList.contains('is-open')) { front = name; render(); say(`${name} is already open; its window comes forward`); return; }
    c.classList.add('is-launching');
    setStatus(c, 'Opening…');
    const done = () => {
      c.classList.remove('is-launching');
      c.classList.add('is-open');
      setStatus(c, 'Open');
      front = name; render(name);
      say(`${name} opened, signed in`);
      // like the app's "Hide after opening an account", so the new window can be seen
      if (wide.matches && !picker.classList.contains('is-hidden')) setTimeout(() => togglePicker(false), 650);
    };
    reduced ? done() : setTimeout(done, 480);
  }

  function closeAccount(name) {
    const c = card(name);
    if (!c) return;
    c.classList.remove('is-open');
    setStatus(c, 'Just now');
    if (front === name) { const other = cards.find((x) => x.classList.contains('is-open')); front = other ? other.dataset.name : ''; }
    render();
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

  function togglePicker(show = picker.classList.contains('is-hidden')) {
    picker.classList.toggle('is-hidden', !show);
    if (hint) hint.textContent = show ? 'Try it: press 1, 2 or 3' : 'Ctrl+Alt+C or the tray icon brings it back';
    say(show ? 'Picker shown' : 'Picker hidden. Press Ctrl+Alt+C to bring it back');
  }

  if (picker) {
    if (matchMedia('(pointer: coarse)').matches && hint) hint.textContent = 'Try it: tap a card';
    cards.forEach((c) => c.addEventListener('click', (e) => {
      if (e.ctrlKey || e.metaKey) { picked.has(c) ? picked.delete(c) : picked.add(c); updatePicked(); return; }
      picked.clear(); updatePicked();
      openAccount(c);
    }));
    many && many.addEventListener('click', () => { picked.forEach((c) => openAccount(c)); picked.clear(); updatePicked(); });
    $$('[data-toggle-picker]').forEach((t) => t.addEventListener('click', () => togglePicker()));
    $$('[data-close]').forEach((x) => x.addEventListener('click', (e) => { e.stopPropagation(); closeAccount(x.dataset.close); }));
    Object.entries(wins).forEach(([name, w]) => w.addEventListener('pointerdown', () => { if (front !== name) { front = name; render(); } }));

    document.addEventListener('keydown', (e) => {
      const tag = (e.target.tagName || '').toLowerCase();
      if (tag === 'input' || tag === 'textarea' || listening) return;
      if (e.ctrlKey && e.altKey && e.code === 'KeyC') { e.preventDefault(); togglePicker(); return; }
      const n = /^Digit([1-9])$/.exec(e.code);
      if (!n) return;
      const c = cards[Number(n[1]) - 1];
      if (!c) return;
      if (e.ctrlKey && e.altKey) { e.preventDefault(); openAccount(c); return; }
      const r = picker.getBoundingClientRect();
      if (e.ctrlKey || e.altKey || e.metaKey || picker.classList.contains('is-hidden') || r.bottom < 0 || r.top > innerHeight) return;
      openAccount(c);
    });
    render();
  }

  // ---------- the settings window ----------
  $$('.switch').forEach((s) => s.addEventListener('click', () => {
    const on = !s.classList.contains('is-on');
    s.classList.toggle('is-on', on);
    s.setAttribute('aria-checked', String(on));
  }));

  $$('.seg').forEach((seg) => {
    const opts = $$('[role="radio"]', seg);
    const pick = (b, focus) => {
      opts.forEach((o) => { const on = o === b; o.setAttribute('aria-checked', String(on)); o.tabIndex = on ? 0 : -1; });
      if (focus) b.focus();
    };
    opts.forEach((b, i) => {
      b.tabIndex = b.getAttribute('aria-checked') === 'true' ? 0 : -1;
      b.addEventListener('click', () => pick(b));
      b.addEventListener('keydown', (e) => {
        const d = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
        if (d) { e.preventDefault(); pick(opts[(i + d + opts.length) % opts.length], true); }
      });
    });
  });

  // the shortcut field records a real key combination, as the app's does
  const hotkey = $('[data-hotkey]');
  const note = $('[data-shortcut-note]');
  let listening = false;
  if (hotkey) {
    const base = note.textContent;
    const stop = (label) => { listening = false; hotkey.classList.remove('is-listening'); if (label) hotkey.textContent = label; };
    hotkey.addEventListener('click', () => {
      listening = true;
      hotkey.classList.add('is-listening');
      hotkey.textContent = 'Press keys…';
      note.textContent = 'Press a shortcut, for example Ctrl + Alt + C. Esc cancels';
    });
    hotkey.addEventListener('blur', () => { if (listening) { stop(hotkey.dataset.last || 'Ctrl + Alt + C'); note.textContent = base; } });
    hotkey.addEventListener('keydown', (e) => {
      if (!listening) return;
      e.preventDefault();
      if (e.key === 'Escape') { stop(hotkey.dataset.last || 'Ctrl + Alt + C'); note.textContent = base; return; }
      if (['Control', 'Alt', 'Shift', 'Meta'].includes(e.key)) return;
      const mods = [e.ctrlKey && 'Ctrl', e.altKey && 'Alt', e.shiftKey && 'Shift', e.metaKey && 'Win'].filter(Boolean);
      if (!mods.length || (mods.length === 1 && mods[0] === 'Shift')) { note.textContent = 'Add Ctrl, Alt or Win so the shortcut doesn’t get in the way of typing'; return; }
      const key = e.code.startsWith('Key') ? e.code.slice(3) : e.code.startsWith('Digit') ? e.code.slice(5) : e.key.length === 1 ? e.key.toUpperCase() : e.key;
      const label = [...mods, key].join(' + ');
      hotkey.dataset.last = label;
      stop(label);
      note.textContent = base;
    });
  }

  // ---------- download: the build follows the choice ----------
  const dl = $('[data-download]');
  const dlLabel = $('[data-download-label]');
  const radios = $$('input[name="arch"]');
  const setArch = (arch) => {
    dl.href = `https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-${arch}.exe`;
    dlLabel.textContent = `Download for ${arch === 'arm64' ? 'ARM64' : 'x64'}`;
  };
  radios.forEach((r) => r.addEventListener('change', () => setArch(r.value)));
  // Windows on ARM reports itself through client hints; preselect it when it does
  if (navigator.userAgentData && navigator.userAgentData.getHighEntropyValues) {
    navigator.userAgentData.getHighEntropyValues(['architecture', 'platform']).then((v) => {
      if (v.platform === 'Windows' && v.architecture === 'arm') { const r = radios.find((x) => x.value === 'arm64'); r.checked = true; setArch('arm64'); }
    }).catch(() => {});
  }

  // ---------- the title bar: a marker slides to the hovered link and rests on the section in view ----------
  const pivot = $('.pivot');
  const links = $$('.pivot a');
  const ink = $('.pivot__ink');
  const inkTo = (a) => {
    if (!ink) return;
    if (!a) { pivot.style.setProperty('--io', 0); return; }
    pivot.style.setProperty('--ix', `${a.offsetLeft + a.offsetWidth / 2 - 8}px`);
    pivot.style.setProperty('--io', 1);
  };
  const activeLink = () => links.find((a) => a.classList.contains('is-active'));
  links.forEach((a) => { a.addEventListener('pointerenter', () => inkTo(a)); a.addEventListener('focus', () => inkTo(a)); });
  pivot && pivot.addEventListener('pointerleave', () => inkTo(activeLink()));
  pivot && pivot.addEventListener('focusout', () => inkTo(activeLink()));
  const secs = links.map((a) => $(a.getAttribute('href'))).filter(Boolean);
  if ('IntersectionObserver' in window && secs.length) {
    const spy = new IntersectionObserver((es) => es.forEach((en) => {
      if (!en.isIntersecting) return;
      links.forEach((a) => a.classList.toggle('is-active', a.getAttribute('href') === '#' + en.target.id));
      if (!pivot.matches(':hover')) inkTo(activeLink());
    }), { rootMargin: '-45% 0px -50% 0px' });
    secs.forEach((s) => spy.observe(s));
  }

  // ---------- primary buttons fill from the side the pointer came in ----------
  $$('[data-fill]').forEach((b) => {
    const at = (e) => { const r = b.getBoundingClientRect(); b.style.setProperty('--fx', `${e.clientX - r.left}px`); b.style.setProperty('--fy', `${e.clientY - r.top}px`); };
    b.addEventListener('pointerenter', (e) => { if (e.pointerType !== 'mouse') return; at(e); b.classList.add('is-filled'); });
    b.addEventListener('pointerleave', (e) => { at(e); b.classList.remove('is-filled'); });
  });

  // ---------- Fluent reveal light: borders light up where the pointer is ----------
  if (matchMedia('(hover: hover)').matches) {
    let lit = null;
    document.addEventListener('pointermove', (e) => {
      const el = e.target.closest && e.target.closest('.tile, .row, .expanders details, .flow__node');
      if (lit && lit !== el) lit = null;
      if (!el) return;
      lit = el;
      const r = el.getBoundingClientRect();
      el.style.setProperty('--mx', `${e.clientX - r.left}px`);
      el.style.setProperty('--my', `${e.clientY - r.top}px`);
    }, { passive: true });
  }

  // ---------- reveals: once, staggered within a group ----------
  $$('.bento .tile').forEach((el, i) => el.style.setProperty('--d', i % 3));
  const targets = $$('[data-reveal]');
  if (!('IntersectionObserver' in window)) { targets.forEach((el) => el.classList.add('is-in')); return; }
  const io = new IntersectionObserver((entries) => {
    for (const en of entries) { if (en.isIntersecting) { en.target.classList.add('is-in'); io.unobserve(en.target); } }
  }, { rootMargin: '0px 0px -8% 0px', threshold: 0.06 });
  targets.forEach((el) => io.observe(el));
  addEventListener('load', () => setTimeout(() => {
    targets.forEach((el) => { const r = el.getBoundingClientRect(); if (r.top < innerHeight && r.bottom > 0) el.classList.add('is-in'); });
  }, 2500));
})();
