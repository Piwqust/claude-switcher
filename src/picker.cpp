#include "app.h"
#include <cmath>

using namespace Gfx;

namespace {
    const float W = 612, Pad = 24, TileW = 132, Gap = 12;

    // Cards grow a limits section only when some account has current figures to show.
    float TileH() { for (auto& i : App::items) if (i.p.usage.known) return 204; return 152; }

    enum class Banner { None, Countdown, NewAccount };

    struct PickerWin : Surface {
        Banner banner = Banner::None;
        LARGE_INTEGER countdownEnd{}; double countdownTotal = 0;
        std::wstring newAccount;
        RectF bannerButton;
        std::wstring renaming;     // id whose name is being edited in place

        void Start() {
            sizeToContent = true;
            backdrop = 2;
            Create(AppTitle(), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, W, 420);
        }

        // ------------------------------------------------------------ state

        std::vector<Item*> Checked() { std::vector<Item*> v; for (auto& i : App::items) if (i.checked) v.push_back(&i); return v; }
        bool SelectMode() { for (auto& i : App::items) if (i.checked) return true; return false; }
        bool CountingDown() const { return banner == Banner::Countdown; }
        void ClearSelection() { for (auto& i : App::items) i.checked = false; Invalidate(); }

        void StartCountdown(const std::vector<std::wstring>& ids, int seconds) {
            for (auto& i : App::items) i.checked = std::find(ids.begin(), ids.end(), i.p.id) != ids.end();
            if (Checked().empty()) return;
            LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
            countdownEnd.QuadPart = now.QuadPart + (LONGLONG)(seconds * f.QuadPart);
            countdownTotal = seconds;
            banner = Banner::Countdown;
            Invalidate();
        }

        double SecondsLeft() {
            LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
            return (countdownEnd.QuadPart - now.QuadPart) / (double)f.QuadPart;
        }

        void FinishCountdown() {
            std::vector<std::wstring> ids;
            for (auto i : Checked()) ids.push_back(i->p.id);
            banner = Banner::None;
            App::Launch(ids, true);
        }

        void CancelCountdown() { if (!CountingDown()) return; banner = Banner::None; ClearSelection(); }

        Item* ItemByTile(UINT id) { for (auto& i : App::items) if (Id("tile", i.index) == id) return &i; return nullptr; }

        void Activate(Item& it) {
            if (Editing()) return;
            // Once anything is ticked, a click ticks too: a selection is being built.
            if ((GetKeyState(VK_CONTROL) & 0x8000) || SelectMode()) { it.checked = !it.checked; Invalidate(); }
            else App::Launch({ it.p.id }, true);
        }

        // The menu runs its own loop; whatever the pointer did meanwhile went unseen, so hover
        // is re-read from where the pointer is now.
        void Menu(Item& it, POINT at) { App::ShowMenu(it.p.id, at, hwnd); RefreshHover(); }

        // Same geometry as Build(), for placing things on a card from outside it.
        RectF TileRect(int index) {
            int i = index - 1, cols = 4;
            float x0 = (W - (cols * TileW + (cols - 1) * Gap)) / 2, top = 58 + (banner != Banner::None ? 76.f : 0.f);
            return { x0 + (i % cols) * (TileW + Gap), top + (i / cols) * (TileH() + Gap), TileW, TileH() };
        }
        static RectF NameField(RectF tile) { return { tile.x + 10, tile.y + 93, tile.w - 20, 24 }; }

        POINT Screen(float x, float y) { POINT p{ (LONG)(x * scale), (LONG)(y * scale) }; ClientToScreen(hwnd, &p); return p; }

        // ------------------------------------------------------------ drawing

        void BannerUi(float y) {
            RectF r{ Pad, y, W - 2 * Pad, 64 };
            Fill(r, T.accentTint, 6);
            const wchar_t* glyph = CountingDown() ? Glyphs::Timer : Glyphs::Contact;
            Glyph(glyph, { r.x + 12, r.y + 12, 24, 24 }, 16, T.accentText);
            std::wstring title, text, button;
            if (CountingDown()) {
                double left = SecondsLeft();
                if (left <= 0) { FinishCountdown(); return; }
                std::vector<std::wstring> names; for (auto i : Checked()) names.push_back(i->name);
                title = L10n::ru ? Util::Format(L"Через %d с %s %s", (int)std::ceil(left), names.size() == 1 ? L"откроется" : L"откроются", Util::Names(names).c_str())
                                 : Util::Format(L"Opening %s in %d s", Util::Names(names).c_str(), (int)std::ceil(left));
                text = Tr(L"Press any key to choose yourself", L"Нажмите любую клавишу, чтобы выбрать самостоятельно");
                button = Tr(L"Open now", L"Открыть сейчас");
                float w = (float)(r.w * left / countdownTotal);
                Fill({ r.x, r.B() - 3, w, 3 }, T.accent, 1.5f);
                Animate();
            } else {
                auto it = App::Find(newAccount);
                title = Tr(L"Sign in in the new Claude window", L"Войдите в аккаунт в новом окне Claude");
                text = Util::Format(Tr(L"The name %s is selected: type your own, like “Work”", L"Имя %s уже выделено: впишите своё, например «Работа»"), Util::Q(it ? it->name : newAccount).c_str());
                button = Tr(L"Done", L"Готово");
            }
            float bw = Measure(button, Font(14)).width + 26;
            bannerButton = { r.R() - bw - 12, r.y + 16, bw, 32 };
            Text(title, { r.x + 44, r.y + 12, r.w - 44 - bw - 24, 20 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text);
            Text(text, { r.x + 44, r.y + 33, r.w - 44 - bw - 24, 18 }, Font(12), T.text2);
            Button(Id("banner"), bannerButton, button, false, [this] {
                if (CountingDown()) FinishCountdown(); else { banner = Banner::None; Invalidate(); }
            });
        }

        void UsageRow(float x, float y, float w, const wchar_t* label, double pct, bool stale) {
            auto lc = pct < 0 || stale ? T.text3 : T.text2;
            Text(label, { x, y, 28, 14 }, Font(11), T.text3);
            Bar({ x + 28, y + 5, w - 28 - 34, 4 }, pct, stale);
            Text(pct < 0 ? L"—" : Util::Format(L"%d%%", (int)std::lround(pct)), { x + w - 32, y, 32, 14 }, Font(11), lc, Align::Right);
        }


        void Tile(Item& it, RectF r) {
            UINT id = Id("tile", it.index);
            // Hover is "the pointer is over the card", not "the card is the hit region under
            // the pointer": the tick and ⋯ are regions of their own, and treating them as
            // leaving the card made them hide themselves, flicker and dodge the pointer.
            bool over = Over(r);
            float h = A(id ^ 0x51, over ? 1.f : 0.f), p = PressT(id);
            bool select = SelectMode();
            RectF b = p > 0 ? r.Inset(r.w * .01f * p) : r;
            Fill(b, Mix(Mix(T.layer, T.layerHover, h), T.layerPress, p), 8);
            float c = A(id ^ 0xC4, it.checked ? 1.f : 0.f, 120);
            auto strokeHover = T.dark ? Rgba(0xFFFFFF, .16f) : Rgba(0x000000, .14f);
            Stroke(b, Mix(Mix(T.layerStroke, strokeHover, h), T.accent, c), 8, 1 + c);
            // Handlers look the account up again by id: the list may be rebuilt before a click lands.
            auto pid = it.p.id;
            Hit(id, r, [this, pid] { if (auto x = App::Find(pid)) Activate(*x); },
                       [this, pid] { POINT pt; GetCursorPos(&pt); if (auto x = App::Find(pid)) Menu(*x, pt); });
            FocusRing(id, r, 8);

            // Top-left: the digit, or a tick box while hovering or selecting.
            float showTick = A(id ^ 0x71, (over || select) ? 1.f : 0.f, 120);
            if (!it.p.id.empty() && it.index <= 9 && showTick < 1) {
                PushOpacity(1 - showTick);
                auto key = std::to_wstring(it.index);
                Fill({ r.x + 10, r.y + 10, 20, 20 }, T.badge, 4);
                Text(key, { r.x + 10, r.y + 10, 20, 20 }, Font(12, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text2, Align::Center);
                PopOpacity();
            }
            if (showTick > 0) {
                PushOpacity(showTick);
                UINT tid = Id("tick", it.index);
                float ct = c;
                Circle(r.x + 20, r.y + 20, 10, Mix(T.control, T.accent, ct));
                if (ct < 1) { auto s = T.controlStrong; s.a *= 1 - ct; Ring(r.x + 20, r.y + 20, 10, s, 1.5f); }
                if (ct > 0) { auto oc = T.onAccent; oc.a = ct; Glyph(Glyphs::Check, { r.x + 10, r.y + 10, 20, 20 }, 11, oc); }
                Hit(tid, { r.x + 4, r.y + 4, 32, 32 }, [this, pid] { if (auto x = App::Find(pid)) { x->checked = !x->checked; Invalidate(); } }, nullptr, false);
                PopOpacity();
            }
            // Top-right: the menu, shown on hover or keyboard focus.
            float showMore = A(id ^ 0x6D, (over || (keyboardCues && focus == id)) ? 1.f : 0.f, 120);
            if (showMore > 0.01f) {
                PushOpacity(showMore);
                UINT mid = Id("more", it.index);
                RectF m{ r.R() - 34, r.y + 6, 28, 28 };
                float mh = HotT(mid);
                Fill(m, Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, mh), 4);
                Glyph(Glyphs::More, m, 14, T.text2);
                Hit(mid, m, [this, pid, m] { if (auto x = App::Find(pid)) Menu(*x, Screen(m.x, m.B())); }, nullptr, false);
                PopOpacity();
            }

            Avatar(r.x + r.w / 2, r.y + 58, 56, it.letters, it.color, it.p.isDefault, it.running(), T.dark ? Rgba(0x2B2B2B) : Rgba(0xFBFBFB));
            if (Editing() && renaming == it.p.id) {
                // The text box sits here; this is its field, drawn in the Windows 11 style.
                RectF f = NameField(r);
                Fill(f.Inset(-1), T.menu, 4);
                Stroke(f.Inset(-1), T.controlStroke, 4);
                Fill({ f.x - 1, f.B() - 1, f.w + 2, 2 }, T.accent, 1);
            } else
                Text(it.name, { r.x + 8, r.y + 94, r.w - 16, 22 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text, Align::Center);
            auto statusColor = it.launching ? T.accentText : it.running() ? T.success : T.text2;
            Text(it.Status(), { r.x + 8, r.y + 116, r.w - 16, 18 }, Font(12), statusColor, Align::Center);

            // Plan usage, only when current; otherwise the section is not drawn at all.
            auto& u = it.p.usage;
            if (u.known) {
                float ux = r.x + 14, uw = r.w - 28;
                Line(ux, r.y + 142, ux + uw, r.y + 142, T.layerStroke);
                UsageRow(ux, r.y + 150, uw, Tr(L"5 h", L"5 ч"), u.fiveHour, false);
                UsageRow(ux, r.y + 167, uw, Tr(L"7 d", L"7 дн"), u.week, false);
                Text(Tr(L"Updated ", L"Обновлено ") + Core::Age(u.newest()), { r.x + 6, r.y + 184, r.w - 12, 14 }, Font(10.5f), T.text3, Align::Center);
            }
        }

        void AddTile(RectF r) {
            UINT id = Id("addtile");
            float h = HotT(id), p = PressT(id);
            Fill(r, Mix(D2D1::ColorF(0, 0, 0, 0), T.layer, h), 8);
            auto s = T.controlStrong; s.a *= .6f;
            DashedStroke(p > 0 ? r.Inset(1) : r, s, 8);
            Circle(r.x + r.w / 2, r.y + 58, 28, T.badge);
            Glyph(Glyphs::Add, { r.x + r.w / 2 - 28, r.y + 30, 56, 56 }, 20, T.text2);
            Text(Tr(L"Add", L"Добавить"), { r.x, r.y + 94, r.w, 22 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text, Align::Center);
            Text(Tr(L"New sign-in", L"Новый вход"), { r.x, r.y + 116, r.w, 18 }, Font(12), T.text2, Align::Center);
            if (r.h > 170) Text(L"Ctrl+N", { r.x, r.y + 152, r.w, 18 }, Font(11), T.text3, Align::Center);
            Hit(id, r, [] { App::AddAccount(); });
            FocusRing(id, r, 8);
        }

        void Build() override {
            Text(Tr(L"Choose an account", L"Какой аккаунт открыть?"), { Pad, 14, W - 2 * Pad - 40, 32 }, Font(20, DWRITE_FONT_WEIGHT_SEMI_BOLD, true), T.text);
            IconButton(Id("settings"), { W - Pad - 32, 14, 32, 32 }, Glyphs::Settings, [] { App::ShowSettings(); });

            float y = 58;
            if (banner != Banner::None) { BannerUi(y); y += 64 + 12; }

            int n = (int)App::items.size() + 1, cols = 4;
            float gridW = cols * TileW + (cols - 1) * Gap, x0 = (W - gridW) / 2;
            for (int i = 0; i < n; i++) {
                RectF r{ x0 + (i % cols) * (TileW + Gap), y + (i / cols) * (TileH() + Gap), TileW, TileH() };
                if (i < (int)App::items.size()) Tile(App::items[i], r); else AddTile(r);
            }
            y += ((n + cols - 1) / cols) * (TileH() + Gap) - Gap + 18;

            // Footer: the sign-in switch, and either a hint or the open-selected bar.
            bool show = App::ShowAtStartup();
            Toggle(Id("startup"), Pad, y + 6, show, [] { App::SetShowAtStartup(!App::ShowAtStartup()); Picker::Invalidate(); });
            Text(Tr(L"Show when Windows starts", L"Показывать при входе в Windows"), { Pad + 50, y, 240, 32 }, Font(13), T.text2);
            Hit(Id("startuplabel"), { Pad + 50, y, 220, 32 }, [] { App::SetShowAtStartup(!App::ShowAtStartup()); Picker::Invalidate(); }, nullptr, false);

            auto picked = Checked();
            if (!picked.empty() && !CountingDown()) {
                auto label = picked.size() == 1 ? Util::Format(Tr(L"Open %s", L"Открыть %s"), Util::Q(picked[0]->name).c_str())
                                             : Tr(L"Open ", L"Открыть ") + Util::Count((int)picked.size(), L"account", L"accounts", L"аккаунт", L"аккаунта", L"аккаунтов");
                float bw = std::min(260.f, Measure(label, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD)).width + 28);
                Button(Id("open-selected"), { W - Pad - bw, y, bw, 32 }, label, true, [this] {
                    std::vector<std::wstring> ids; for (auto i : Checked()) ids.push_back(i->p.id);
                    App::Launch(ids, true);
                });
                RectF clear{ W - Pad - bw - 96, y, 88, 32 };
                float ch = HotT(Id("clear"));
                Fill(clear, Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, ch), 4);
                Text(Tr(L"Clear", L"Сбросить"), clear, Font(14), T.text2, Align::Center);
                Hit(Id("clear"), clear, [this] { ClearSelection(); });
            } else {
                auto hint = Tr(L"Press a number to open, Ctrl+click to pick several", L"Цифра — открыть, Ctrl+клик — выбрать несколько");
                Text(hint, { W - Pad - 300, y, 300, 32 }, Font(12), T.text3, Align::Right);
            }
            contentHeight = y + 32 + 22;
        }

        // ------------------------------------------------------------ input

        bool OnKey(UINT vk, bool alt) override {
            if (CountingDown()) { CancelCountdown(); if (vk == VK_ESCAPE) return true; }
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (vk >= '1' && vk <= '9' && !ctrl && !alt) { int d = vk - '0'; for (auto& i : App::items) if (i.index == d) App::Launch({ i.p.id }, true); return true; }
            if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9 && !ctrl) { int d = vk - VK_NUMPAD0; for (auto& i : App::items) if (i.index == d) App::Launch({ i.p.id }, true); return true; }
            if (ctrl && vk == 'N') { App::AddAccount(); return true; }
            if (ctrl && vk == VK_OEM_COMMA) { App::ShowSettings(); return true; }
            auto focused = ItemByTile(focus);
            if (vk == VK_ESCAPE) { if (SelectMode()) ClearSelection(); else OnClose(); return true; }
            if (vk == VK_RETURN && SelectMode() && !focused) { std::vector<std::wstring> ids; for (auto i : Checked()) ids.push_back(i->p.id); App::Launch(ids, true); return true; }
            if (!focused) return false;
            if (vk == VK_SPACE) { focused->checked = !focused->checked; Invalidate(); return true; }
            if (vk == VK_F2) { App::BeginRename(focused->p.id); return true; }
            if (vk == VK_DELETE) { App::Delete(focused->p.id, hwnd); return true; }
            if (vk == VK_APPS || (vk == VK_F10 && (GetKeyState(VK_SHIFT) & 0x8000))) {
                auto t = TileRect(focused->index);
                Menu(*focused, Screen(t.R() - 34, t.y + 34));
                return true;
            }
            return false;
        }

        void OnMouseDown() override {
            if (CountingDown() && !bannerButton.Has(mx, my)) CancelCountdown();
        }

        void OnClose() override { App::ClosePicker(); }

        void FocusLikely() {
            Item* best = nullptr;
            for (auto& i : App::items) if (!best || (i.running() && !best->running()) || (i.running() == best->running() && i.p.lastUsed > best->p.lastUsed)) best = &i;
            if (best) focus = Id("tile", best->index);
            keyboardCues = false;
        }

        void BeginRename(Item& it) {
            auto id = it.p.id;
            renaming = id;
            ShowEdit(NameField(TileRect(it.index)), it.name, [this, id](bool commit, const std::wstring& text) {
                renaming.clear();
                if (commit) App::CommitRename(id, text);
            });
            Invalidate();
        }
    };

    PickerWin* win = nullptr;
    PickerWin& Get() { if (!win) { win = new PickerWin(); win->Start(); } return *win; }
}

namespace Picker {
    void Show(bool atSignIn, const std::vector<std::wstring>& autoOpen) {
        auto& w = Get();
        if (w.CountingDown() && !atSignIn) w.CancelCountdown();
        w.ClearSelection();
        if (atSignIn && !autoOpen.empty() && App::Countdown() > 0) w.StartCountdown(autoOpen, App::Countdown());
        if (!w.Visible()) {
            // Centred on the monitor the pointer is on, where the person is looking.
            POINT pt; GetCursorPos(&pt);
            MONITORINFO mi{ sizeof mi }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
            RECT wr; GetWindowRect(w.hwnd, &wr);
            int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
            SetWindowPos(w.hwnd, nullptr, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2,
                         mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2 - 40, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            w.FocusLikely();
        }
        if (IsIconic(w.hwnd)) ShowWindow(w.hwnd, SW_RESTORE);
        w.Show(true);
    }
    void Close() { if (win) { win->CancelCountdown(); win->Hide(); } }
    bool Visible() { return win && win->Visible(); }
    bool Active() { return win && GetForegroundWindow() == win->hwnd; }
    void NewAccountHint(const std::wstring& id) { auto& w = Get(); w.banner = Banner::NewAccount; w.newAccount = id; w.Invalidate(); }
    void Rename(const std::wstring& id) {
        auto& w = Get();
        if (auto it = App::Find(id)) {
            if (!w.Visible()) Show(false, {});
            SetForegroundWindow(w.hwnd);
            UpdateWindow(w.hwnd);
            w.BeginRename(*it);
        }
    }
    void Restyle() { if (win) win->Restyle(); }
    void Invalidate() { if (win) win->Invalidate(); }
    HWND Handle() { return win ? win->hwnd : nullptr; }
}
