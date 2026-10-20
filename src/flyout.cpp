#include "app.h"
#include <shellapi.h>
#include <cmath>

using namespace Gfx;

namespace {
    const float W = 320, RowH = 52;

    struct FlyoutWin : Surface {
        ULONGLONG hiddenAt = 0;
        LARGE_INTEGER shownAt{};
        bool fromBelow = true;

        void Start() {
            sizeToContent = true;
            backdrop = 3;   // Acrylic, like the system's own tray flyouts
            Create(Tr(L"Account list", L"Список аккаунтов"), WS_POPUP, WS_EX_TOOLWINDOW | WS_EX_TOPMOST, W, 300, nullptr, true);
        }

        float Appear() {
            // Slides in 10 px and fades, over ~180 ms, as Windows 11 flyouts do.
            LARGE_INTEGER now, f; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&f);
            float t = (float)((now.QuadPart - shownAt.QuadPart) * 1000.0 / f.QuadPart) / 180.f;
            if (t >= 1) return 1;
            Animate();
            return 1 - (1 - t) * (1 - t) * (1 - t);
        }

        // "5 ч 40% · 7 дн 12% · в 07:42": limits with the time they were taken.
        std::wstring Limits(const Item& it, float maxW) {
            auto& u = it.p.usage;
            if (!u.known || (u.fiveHour < 0 && u.week < 0)) return L"";
            std::wstring s = Tr(L"5 h ", L"5 ч ");
            s += u.fiveHour >= 0 ? Util::Format(L"%d%%", (int)std::lround(u.fiveHour)) : L"—";
            s += Tr(L" · 7 d ", L" · 7 дн ");
            s += u.week >= 0 ? Util::Format(L"%d%%", (int)std::lround(u.week)) : L"—";
            // The row is narrow: the time goes only when it fits whole, older dates and the rest are on the card.
            auto age = Core::Age(u.newest());
            auto withAge = s + L" · " + age;
            return age.find(L',') == std::wstring::npos && Measure(L"·  " + withAge, Font(12)).width <= maxW ? withAge : s;
        }

        void Row(UINT id, float y, std::function<void()> click) {
            RectF r{ 4, y, W - 8, RowH };
            float h = HotT(id), p = PressT(id);
            Fill(r, Mix(Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, h), T.layerPress, p), 4);
            Hit(id, r, click);
            FocusRing(id, r, 4);
        }

        void Build() override {
            float t = Appear();
            PushOpacity(t);
            Translate(0, (fromBelow ? 10 : -10) * (1 - t));
            Stroke(Client(), T.menuStroke, 8);

            Text(AppTitle(), { 16, 10, 200, 32 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text);
            IconButton(Id("fo-open"), { W - 80, 10, 32, 32 }, Glyphs::Open, [this] { Hide(); App::ShowPicker(); });
            IconButton(Id("fo-settings"), { W - 44, 10, 32, 32 }, Glyphs::Settings, [this] { Hide(); App::ShowSettings(); });

            float y = 48;
            for (auto& it : App::items) {
                UINT id = Id("fo-row", it.index);
                auto pid = it.p.id;
                Row(id, y, [this, pid] { Hide(); App::Launch({ pid }, false); });
                // An open account gets a restart button between its text and the shortcut digit.
                bool restartable = it.running() && !it.launching;
                float tw = restartable ? W - 140 : W - 110;
                Avatar(30, y + RowH / 2, 32, it.letters, it.color, it.p.isDefault, it.running(), T.dark ? Rgba(0x2C2C2C) : Rgba(0xF3F3F3));
                Text(it.name, { 56, y + 8, tw, 20 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text);
                auto status = it.Status();
                float sw = Measure(status, Font(12)).width;
                Text(status, { 56, y + 27, tw, 18 }, Font(12), it.launching ? T.accentText : it.running() ? T.success : T.text2);
                auto lim = Limits(it, tw - sw - 6);
                if (!lim.empty()) Text(L"·  " + lim, { 56 + sw + 6, y + 27, tw - sw - 6, 18 }, Font(12), T.text3);
                if (restartable) IconButton(Id("fo-restart", it.index), { W - 78, y + 10, 32, 32 }, Glyphs::Restart, [pid] { App::Restart({ pid }); });
                if (it.index <= 9) {
                    RectF b{ W - 40, y + 16, 20, 20 };
                    Fill(b, T.badge, 4);
                    Text(std::to_wstring(it.index), b, Font(12), T.text2, Align::Center);
                }
                y += RowH + 2;
            }
            Line(0, y + 4, W, y + 4, T.menuStroke);
            y += 9;
            Row(Id("fo-add"), y, [this] { Hide(); App::AddAccount(); });
            Glyph(Glyphs::Add, { 14, y, 32, RowH }, 14, T.text2);
            Text(Tr(L"Add account", L"Добавить аккаунт"), { 56, y, 200, RowH }, Font(14), T.text);
            y += RowH;
            Row(Id("fo-exit"), y, [this] { Hide(); App::Exit(); });
            Glyph(Glyphs::Power, { 14, y, 32, RowH }, 14, T.text2);
            Text(Tr(L"Quit switcher", L"Закрыть переключатель"), { 56, y, 220, RowH }, Font(14), T.text);
            y += RowH + 6;
            contentHeight = y;
            PopOpacity();
            Translate(0, 0);
        }

        bool OnKey(UINT vk, bool) override {
            if (vk == VK_ESCAPE) { Hide(); return true; }
            int d = vk >= '1' && vk <= '9' ? vk - '0' : vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9 ? vk - VK_NUMPAD0 : 0;
            for (auto& i : App::items) if (d && i.index == d) { auto id = i.p.id; Hide(); App::Launch({ id }, false); return true; }
            return false;
        }

        void OnDeactivate() override {
            if (GetEnvironmentVariableW(L"CPS_FLYOUT_STAY", nullptr, 0)) return;   // for screenshots only
            if (Visible()) { Hide(); hiddenAt = GetTickCount64(); }
        }
        void OnClose() override { Hide(); }

        void Place() {
            NOTIFYICONIDENTIFIER nid{ sizeof nid }; nid.hWnd = App::tray; nid.uID = 1;
            RECT icon{}; POINT pt;
            if (SUCCEEDED(Shell_NotifyIconGetRect(&nid, &icon))) pt = { (icon.left + icon.right) / 2, (icon.top + icon.bottom) / 2 };
            else GetCursorPos(&pt);
            MONITORINFO mi{ sizeof mi }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
            RECT wr; GetWindowRect(hwnd, &wr);
            int w = wr.right - wr.left, h = wr.bottom - wr.top, gap = (int)(12 * scale);
            int x = std::max((int)mi.rcWork.left + gap, std::min((int)pt.x - w / 2, (int)mi.rcWork.right - w - gap));
            fromBelow = pt.y > (mi.rcWork.top + mi.rcWork.bottom) / 2;
            int y = fromBelow ? mi.rcWork.bottom - h - gap : mi.rcWork.top + gap;
            SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }

        void Open() {
            // Measure first so the placement knows the height, then show.
            RECT rc; GetClientRect(hwnd, &rc);
            InvalidateRect(hwnd, nullptr, FALSE); UpdateWindow(hwnd);
            Place();
            QueryPerformanceCounter(&shownAt);
            Show(true);
            Place();
            App::Refresh();
        }
    };

    FlyoutWin* win = nullptr;
    FlyoutWin& Get() { if (!win) { win = new FlyoutWin(); win->Start(); } return *win; }
}

namespace Flyout {
    void Toggle() {
        auto& w = Get();
        if (w.Visible()) { w.Hide(); return; }
        // Clicking the tray icon while the flyout is open deactivates it first; without
        // this, that click would reopen it straight away.
        if (GetTickCount64() - w.hiddenAt < 300) return;
        w.Open();
    }
    void Hide() { if (win) win->Hide(); }
    bool Visible() { return win && win->Visible(); }
    void Restyle() { if (win) win->Restyle(); }
    void Invalidate() { if (win) win->Invalidate(); }
}
