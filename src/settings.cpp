#include "app.h"
#include <shellapi.h>

using namespace Gfx;

namespace {
    const float Pad = 24;

    struct SettingsW : Surface {
        bool recording = false;
        std::wstring hotkeyNote;
        bool hotkeyNoteBad = false;

        void Start() {
            backdrop = 2;
            Create(Title(), WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, 0, 600, 720);
        }

        static const wchar_t* Title() { return Tr(L"Settings — Claude Accounts", L"Настройки — Аккаунты Claude"); }
        float Wd() { return Client().w; }

        // A Windows 11 settings row: title and description on the left, a control on the right.
        // extra: room below the text for content such as a list; returned height excludes it.
        float Card(float y, const std::wstring& title, const std::wstring& desc, float controlW, float minH = 68, D2D1_COLOR_F descColor = {}, float extra = 0) {
            float w = Wd() - 2 * Pad, textW = w - 32 - controlW - (controlW > 0 ? 16 : 0);
            float dh = desc.empty() ? 0 : Measure(desc, Font(12), textW, true).height;
            float h = std::max(minH, 24 + 20 + (dh ? dh + 2 : 0));
            RectF r{ Pad, y, w, h + extra };
            Fill(r, T.card, 6); Stroke(r, T.cardStroke, 6);
            float ty = y + (h - 20 - (dh ? dh + 2 : 0)) / 2;
            Text(title, { r.x + 16, ty, textW, 20 }, Font(14), T.text);
            if (dh) Text(desc, { r.x + 16, ty + 22, textW, dh }, Font(12), descColor.a > 0 ? descColor : T.text2, Align::Left, false, true);
            return h;
        }

        float Section(float y, const wchar_t* t) { Text(t, { Pad + 2, y + 20, 300, 20 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text); return 48; }

        float ToggleRow(float y, const std::wstring& title, const std::wstring& desc, bool on, std::function<void()> flip, bool enabled = true) {
            float h = Card(y, title, desc, 40);
            UINT id = Id("set", (int)std::hash<std::wstring>()(title));
            Toggle(id, Wd() - Pad - 16 - 40, y + h / 2 - 10, on, [this, flip] { flip(); Invalidate(); }, enabled);
            return h + 4;
        }

        void Build() override {
            float y = 16 - scrollY, w = Wd();
            Text(Tr(L"Settings", L"Настройки"), { Pad, y, 300, 40 }, Font(28, DWRITE_FONT_WEIGHT_SEMI_BOLD, true), T.text);
            y += 44;

            // ---------------------------------------------------------------- startup
            y += Section(y, Tr(L"Startup", L"Запуск"));
            y += ToggleRow(y, Tr(L"Start with Windows", L"Запускать вместе с Windows"), Tr(L"The switcher starts when you sign in and waits in the tray", L"Переключатель стартует при входе в систему и ждёт в трее"),
                           App::Autostart(), [] { App::SetAutostart(!App::Autostart()); });
            y += ToggleRow(y, Tr(L"Ask at sign-in", L"Окно выбора при входе"), Tr(L"Asks which account to open. Without it, the accounts ticked below open silently", L"Спросит, какой аккаунт открыть. Без него отмеченные ниже аккаунты откроются молча"),
                           App::AskAtStartup(), [] { App::SetAskAtStartup(!App::AskAtStartup()); }, App::Autostart());

            {
                float rowsH = App::items.size() * 30.f;
                float h = Card(y, Tr(L"Open at sign-in", L"Открывать при входе"), Tr(L"These accounts open on their own. With Ask at sign-in on, a countdown runs first and can be stopped", L"Эти аккаунты откроются сами. Если окно выбора включено, сначала пойдёт отсчёт, и его можно прервать"), 0, 0, {}, rowsH + 8);
                RectF card{ Pad, y, w - 2 * Pad, h + rowsH + 8 };
                float ry = y + h - 6;
                for (auto& it : App::items) {
                    UINT id = Id("auto", it.index);
                    auto pid = it.p.id;
                    float hh = HotT(id);
                    RectF row{ Pad + 10, ry, 300, 28 };
                    Fill(row, Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, hh), 4);
                    Check(id, Pad + 16, ry + 4, it.autoOpen, T.control);
                    Text(it.name, { Pad + 46, ry, 250, 28 }, Font(14), T.text);
                    Hit(id, row, [pid] { App::ToggleAutoOpen(pid); });
                    FocusRing(id, row, 4);
                    ry += 30;
                }
                y = card.B() + 4;
            }

            {
                float segW = 4 * 52 + 8;
                float h = Card(y, Tr(L"Countdown", L"Отсчёт перед открытием"), Tr(L"How long to wait before opening the ticked accounts", L"Сколько ждать, прежде чем открыть отмеченные аккаунты"), segW);
                RectF seg{ w - Pad - 16 - segW, y + h / 2 - 17, segW, 34 };
                Fill(seg, T.control, 6); Stroke(seg, T.controlStroke, 6);
                const wchar_t* labels[] = { Tr(L"Off", L"Нет"), Tr(L"3 s", L"3 с"), Tr(L"5 s", L"5 с"), Tr(L"10 s", L"10 с") }; int values[] = { 0, 3, 5, 10 };
                for (int i = 0; i < 4; i++) {
                    RectF b{ seg.x + 4 + i * 52.f, seg.y + 4, 52, 26 };
                    UINT id = Id("cd", i);
                    bool on = App::Countdown() == values[i];
                    float t = A(id ^ 0x5E, on ? 1.f : 0.f, 140), hh = HotT(id);
                    Fill(b, Mix(Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, hh), T.accent, t), 4);
                    Text(labels[i], b, Font(13), Mix(T.text, T.onAccent, t), Align::Center);
                    int v = values[i];
                    Hit(id, b, [this, v] { App::SetCountdown(v); Invalidate(); });
                    FocusRing(id, b, 4);
                }
                y += h + 4;
            }

            // ---------------------------------------------------------------- quick access
            y += Section(y, Tr(L"Quick access", L"Быстрый доступ"));
            {
                auto hk = App::Hotkey();
                std::wstring desc = recording ? Tr(L"Press a shortcut, for example Ctrl + Alt + C. Esc cancels", L"Нажмите сочетание, например Ctrl + Alt + C. Esc — отмена")
                                  : !hotkeyNote.empty() ? hotkeyNote
                                  : App::HotkeyFailed() ? Tr(L"Another app already uses this shortcut, choose a different one", L"Это сочетание занято другой программой, выберите другое")
                                  : Tr(L"Brings up the account picker from any app", L"Вызывает окно выбора из любой программы");
                bool bad = !recording && (hotkeyNoteBad || (hotkeyNote.empty() && App::HotkeyFailed()));
                float h = Card(y, Tr(L"Shortcut", L"Горячая клавиша"), desc, 200, 68, bad ? T.danger : D2D1_COLOR_F{});
                std::wstring label = recording ? Tr(L"Press keys…", L"Нажмите сочетание…") : hk.empty() ? Tr(L"Not set", L"Не задана") : Hotkeys::Pretty(hk);
                RectF b{ w - Pad - 16 - (hk.empty() ? 160.f : 196.f), y + h / 2 - 16, 160, 32 };
                Button(Id("hk"), b, label, recording, [this] { recording = true; hotkeyNote.clear(); App::SuspendHotkey(); Invalidate(); });
                if (!hk.empty()) IconButton(Id("hk-clear"), { b.R() + 4, b.y, 32, 32 }, Glyphs::Clear, [this] { App::SetHotkey(L""); hotkeyNote.clear(); Invalidate(); });
                y += h + 4;
            }
            {
                auto taken = App::AccountHotkeysTaken();
                std::wstring desc;
                if (!taken.empty() && App::AccountHotkeys()) {
                    desc = Tr(L"Taken by another app:", L"Заняты другой программой:");
                    for (size_t i = 0; i < taken.size(); i++) desc += (i ? L"," : L"") + Util::Format(L" Ctrl + Alt + %d", taken[i]);
                } else {
                    desc = Tr(L"Ctrl + Alt + 1 opens the first account, Ctrl + Alt + 2 the second, and so on. Works from any app", L"Ctrl + Alt + 1 открывает первый аккаунт, Ctrl + Alt + 2 — второй, и так далее. Работает из любой программы");
                }
                float h = Card(y, Tr(L"Ctrl + Alt + number opens an account", L"Ctrl + Alt + цифра открывает аккаунт"), desc, 40, 68, !taken.empty() && App::AccountHotkeys() ? T.danger : D2D1_COLOR_F{});
                Toggle(Id("acchk"), w - Pad - 16 - 40, y + h / 2 - 10, App::AccountHotkeys(), [this] { App::SetAccountHotkeys(!App::AccountHotkeys()); Invalidate(); });
                y += h + 4;
            }
            {
                float h = Card(y, Tr(L"      Accounts on the taskbar", L"      Аккаунты на панели задач"), Tr(L"      Pin the switcher to the taskbar: right-click it to see every account", L"      Закрепите переключатель на панели задач: правый клик по нему покажет все аккаунты"), 0);
                Glyph(Glyphs::Pin, { Pad + 12, y + h / 2 - 12, 24, 24 }, 16, T.text2);
                y += h + 4;
            }

            // ---------------------------------------------------------------- window
            y += Section(y, Tr(L"Window", L"Окно"));
            y += ToggleRow(y, Tr(L"Hide after opening an account", L"Прятать окно после запуска аккаунта"), Tr(L"The picker goes away as soon as Claude opens", L"Окно выбора уберётся, как только откроется Claude"),
                           App::HideAfterLaunch(), [] { App::SetHideAfterLaunch(!App::HideAfterLaunch()); });
            y += ToggleRow(y, Tr(L"Stay in the tray when closed", L"Оставаться в трее после закрытия окна"), Tr(L"Keeps the switcher at hand, so it opens instantly", L"Так переключатель всегда под рукой и открывается мгновенно"),
                           App::KeepInTray(), [] { App::SetKeepInTray(!App::KeepInTray()); });
            y += ToggleRow(y, Tr(L"Separate taskbar buttons per account", L"Отдельные кнопки аккаунтов на панели задач"), Tr(L"Each account gets its own button and icon instead of sharing Claude's. Experimental", L"Каждый аккаунт получит свою кнопку и значок вместо общей кнопки Claude. Экспериментальная функция"),
                           App::SplitTaskbar(), [] { App::SetSplitTaskbar(!App::SplitTaskbar()); });

            // ---------------------------------------------------------------- language
            y += Section(y, Tr(L"Language", L"Язык"));
            {
                float segW = 3 * 84 + 8;
                float h = Card(y, Tr(L"Interface language", L"Язык интерфейса"),
                               Tr(L"System follows Windows: Russian when Windows is in Russian, English otherwise", L"Системный повторяет Windows: русский, если Windows на русском, иначе английский"), segW);
                RectF seg{ w - Pad - 16 - segW, y + h / 2 - 17, segW, 34 };
                Fill(seg, T.control, 6); Stroke(seg, T.controlStroke, 6);
                const wchar_t* labels[] = { Tr(L"System", L"Системный"), L"English", L"Русский" };
                const wchar_t* values[] = { L"auto", L"en", L"ru" };
                auto current = L10n::Choice();
                for (int i = 0; i < 3; i++) {
                    RectF b{ seg.x + 4 + i * 84.f, seg.y + 4, 84, 26 };
                    UINT id = Id("lang", i);
                    float t = A(id ^ 0x5E, current == values[i] ? 1.f : 0.f, 140), hh = HotT(id);
                    Fill(b, Mix(Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, hh), T.accent, t), 4);
                    Text(labels[i], b, Font(13), Mix(T.text, T.onAccent, t), Align::Center);
                    std::wstring v = values[i];
                    Hit(id, b, [this, v] { App::SetLanguage(v); SetWindowTextW(hwnd, Title()); Invalidate(); });
                    FocusRing(id, b, 4);
                }
                y += h + 4;
            }

            // ---------------------------------------------------------------- about
            y += Section(y, Tr(L"About", L"О программе"));
            {
                auto title = L"Claude " + (Core::App.version == L"unknown" ? L"" : Core::App.version + L" ") + L"· " + (Core::App.kind == L"Msix" ? L"Microsoft Store" : Tr(L"installer", L"установщик"));
                float h = Card(y, title, Tr(L"Switcher 1.0. Profiles live in ", L"Переключатель 1.0. Профили лежат в ") + Paths::Root, 150);
                Button(Id("folder"), { w - Pad - 16 - 150, y + h / 2 - 16, 150, 32 }, Tr(L"Profiles folder", L"Папка профилей"), false, [] {
                    ShellExecuteW(nullptr, L"open", Paths::Root.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                });
                y += h + 24;
            }

            float total = y + scrollY;
            scrollMax = std::max(0.f, total - Client().h);
            if (scrollY > scrollMax) { scrollY = scrollMax; Invalidate(); }
            // Thin scroll indicator, as in Windows 11 when the pointer is over the page.
            if (scrollMax > 0) {
                float ch = Client().h, th = std::max(40.f, ch * ch / total), ty = (ch - th) * scrollY / scrollMax;
                Fill({ Client().w - 6, ty + 2, 3, th - 4 }, T.text3, 1.5f);
            }
        }

        bool OnKey(UINT vk, bool alt) override {
            if (!recording) return false;
            if (vk == VK_ESCAPE) { recording = false; App::ResumeHotkey(); Invalidate(); return true; }
            if (vk == VK_CONTROL || vk == VK_MENU || vk == VK_SHIFT || vk == VK_LWIN || vk == VK_RWIN) return true;
            UINT mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (alt || (GetKeyState(VK_MENU) & 0x8000)) mods |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
            if (!(mods & (MOD_CONTROL | MOD_ALT | MOD_WIN))) {
                hotkeyNote = Tr(L"Add Ctrl, Alt or Win so the shortcut doesn't get in the way of typing", L"Добавьте Ctrl, Alt или Win, чтобы сочетание не мешало набору текста"); hotkeyNoteBad = true;
                Invalidate(); return true;
            }
            recording = false;
            bool ok = App::SetHotkey(Hotkeys::FromKeys(mods, vk));
            hotkeyNote = ok ? L"" : Tr(L"Another app already uses this shortcut, choose a different one", L"Это сочетание занято другой программой, выберите другое");
            hotkeyNoteBad = !ok;
            Invalidate();
            return true;
        }

        void OnDeactivate() override { if (recording) { recording = false; App::ResumeHotkey(); } }
        void OnClose() override { if (recording) { recording = false; App::ResumeHotkey(); } Hide(); }
    };

    SettingsW* win = nullptr;
}

namespace SettingsWin {
    void Show() {
        if (!win) {
            win = new SettingsW(); win->Start();
            POINT pt; GetCursorPos(&pt);
            MONITORINFO mi{ sizeof mi }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
            RECT wr; GetWindowRect(win->hwnd, &wr);
            int ww = wr.right - wr.left, wh = std::min<int>(wr.bottom - wr.top, mi.rcWork.bottom - mi.rcWork.top - 40);
            SetWindowPos(win->hwnd, nullptr, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2,
                         mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (IsIconic(win->hwnd)) ShowWindow(win->hwnd, SW_RESTORE);
        win->Show(true);
    }
    bool Visible() { return win && win->Visible(); }
    void Restyle() { if (win) win->Restyle(); }
    void Invalidate() { if (win) win->Invalidate(); }
}
