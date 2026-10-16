#include "app.h"
#include "shell.h"
#include <shellapi.h>
#include <thread>
#include <set>

std::wstring Item::Status() const {
    if (launching) return Tr(L"Starting…", L"Запускается…");
    if (p.pid) return Tr(L"Open", L"Открыт");
    return Util::When(p.lastUsed);
}

namespace App {
    std::vector<Item> items;
    HWND tray = nullptr;
    HICON smallIcon = nullptr;

    enum : UINT { WM_TRAY = WM_APP + 1, WM_REFRESHED, WM_LAUNCHED, WM_ADDED, WM_SIGNAL, WM_RENAME };
    enum : UINT_PTR { TimerRefresh = 1, TimerSoon = 2, TimerHide = 3 };
    const int HotkeyPicker = 1, HotkeyAccount = 10;   // 11..19 for Ctrl+Alt+1..9

    static UINT taskbarCreated;
    static bool refreshing = false, refreshAgain = false, iconAdded = false, hotkeyFailed = false;
    static std::vector<int> accountTaken;
    static std::wstring pendingRename;
    static HANDLE showEvt, exitEvt, addEvt;

    Item* Find(const std::wstring& id) { for (auto& i : items) if (_wcsicmp(i.p.id.c_str(), id.c_str()) == 0) return &i; return nullptr; }

    static void RedrawAll() { Picker::Invalidate(); Flyout::Invalidate(); SettingsWin::Invalidate(); }

    // ------------------------------------------------------------ settings

    bool Autostart() { return Core::AutostartEnabled(); }
    void SetAutostart(bool on) { Core::SetAutostart(on); }
    bool AskAtStartup() { return Settings::Flag(L"AskAtStartup", true); }
    void SetAskAtStartup(bool on) { Settings::Set(L"AskAtStartup", on); }
    // The picker's own switch: shown at sign-in means started with Windows and asking.
    bool ShowAtStartup() { return Autostart() && AskAtStartup(); }
    void SetShowAtStartup(bool on) { if (on && !Autostart()) SetAutostart(true); SetAskAtStartup(on); }
    int Countdown() { return Settings::Int(L"CountdownSeconds", 5); }
    void SetCountdown(int s) { Settings::Set(L"CountdownSeconds", s); }
    bool HideAfterLaunch() { return Settings::Flag(L"HideAfterLaunch", true); }
    void SetHideAfterLaunch(bool on) { Settings::Set(L"HideAfterLaunch", on); }
    bool KeepInTray() { return Settings::Flag(L"KeepInTray", true); }
    void SetKeepInTray(bool on) { Settings::Set(L"KeepInTray", on); }
    bool SplitTaskbar() { return Settings::Flag(L"SplitTaskbar", false); }

    static std::vector<Profile> Profiles() { std::vector<Profile> v; for (auto& i : items) v.push_back(i.p); return v; }

    void SetSplitTaskbar(bool on) { Settings::Set(L"SplitTaskbar", on); WinShell::SplitUpdate(Profiles(), on); }

    // Absent means never set, so the default applies; empty means turned off.
    std::wstring Hotkey() { return Settings::Has(L"Hotkey") ? Settings::Str(L"Hotkey") : L"Ctrl+Alt+C"; }
    bool HotkeyFailed() { return hotkeyFailed; }

    static bool RegisterPicker() {
        UnregisterHotKey(tray, HotkeyPicker);
        UINT mods, vk;
        if (Paths::Demo || !Hotkeys::Parse(Hotkey(), mods, vk)) { hotkeyFailed = false; return true; }
        hotkeyFailed = !RegisterHotKey(tray, HotkeyPicker, mods | MOD_NOREPEAT, vk);
        return !hotkeyFailed;
    }
    bool SetHotkey(const std::wstring& combo) { Settings::Set(L"Hotkey", combo); return RegisterPicker(); }
    void SuspendHotkey() { UnregisterHotKey(tray, HotkeyPicker); }
    void ResumeHotkey() { RegisterPicker(); }

    bool AccountHotkeys() { return Settings::Flag(L"AccountHotkeys", true); }
    std::vector<int> AccountHotkeysTaken() { return accountTaken; }
    static void RegisterAccounts() {
        accountTaken.clear();
        for (int d = 1; d <= 9; d++) UnregisterHotKey(tray, HotkeyAccount + d);
        if (!AccountHotkeys() || Paths::Demo) return;
        for (int d = 1; d <= 9; d++)
            if (!RegisterHotKey(tray, HotkeyAccount + d, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '0' + d)) accountTaken.push_back(d);
    }
    void SetAccountHotkeys(bool on) { Settings::Set(L"AccountHotkeys", on); RegisterAccounts(); }

    void SaveAutoOpen() {
        std::vector<std::wstring> ids;
        for (auto& i : items) if (i.autoOpen) ids.push_back(i.p.id);
        Settings::SetList(L"AutoOpen", ids);
    }

    void ToggleAutoOpen(const std::wstring& id) {
        if (auto it = Find(id)) {
            it->autoOpen = !it->autoOpen;
            SaveAutoOpen();
            if (it->autoOpen && !Autostart()) SetAutostart(true);
            RedrawAll();
        }
    }

    // ------------------------------------------------------------ tray

    static void AddIcon() {
        NOTIFYICONDATAW d{ sizeof d };
        d.hWnd = tray; d.uID = 1; d.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        d.uCallbackMessage = WM_TRAY; d.hIcon = smallIcon;
        wcscpy_s(d.szTip, AppTitle());
        iconAdded = Shell_NotifyIconW(NIM_ADD, &d) != 0;
        d.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &d);
    }

    void Notify(const std::wstring& title, const std::wstring& text, bool error) {
        if (!iconAdded) return;
        NOTIFYICONDATAW d{ sizeof d };
        d.hWnd = tray; d.uID = 1; d.uFlags = NIF_INFO;
        wcsncpy_s(d.szInfoTitle, title.c_str(), _TRUNCATE);
        wcsncpy_s(d.szInfo, text.c_str(), _TRUNCATE);
        d.dwInfoFlags = (error ? NIIF_ERROR : NIIF_INFO) | NIIF_NOSOUND;
        Shell_NotifyIconW(NIM_MODIFY, &d);
    }

    // Everything drawn picks the language up on its next frame; what Windows holds on to
    // (window titles, the tray tip, the jump list, "Main") is updated here.
    void SetLanguage(const std::wstring& choice) {
        L10n::Choose(choice);
        if (iconAdded) {
            NOTIFYICONDATAW d{ sizeof d };
            d.hWnd = tray; d.uID = 1; d.uFlags = NIF_TIP | NIF_SHOWTIP;
            wcscpy_s(d.szTip, AppTitle());
            Shell_NotifyIconW(NIM_MODIFY, &d);
        }
        if (HWND h = Picker::Handle()) SetWindowTextW(h, AppTitle());
        Refresh();
    }

    // The tray's right-click menu: native, so it looks and behaves like every other one.
    static void TrayMenu() {
        WinShell::DarkMenus(Gfx::T.dark);
        HMENU m = CreatePopupMenu();
        int n = 0;
        for (auto& i : items) {
            auto label = i.name + (i.index <= 9 ? Util::Format(L"\tCtrl+Alt+%d", i.index) : L"");
            AppendMenuW(m, MF_STRING | (i.running() ? MF_CHECKED : 0), 100 + n++, label.c_str());
        }
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 1, Tr(L"Show accounts", L"Открыть окно выбора"));
        AppendMenuW(m, MF_STRING, 2, Tr(L"Add account…", L"Добавить аккаунт…"));
        AppendMenuW(m, MF_STRING, 3, Tr(L"Settings", L"Настройки"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 4, Tr(L"Quit switcher", L"Закрыть переключатель"));
        SetMenuDefaultItem(m, 1, FALSE);
        POINT pt; GetCursorPos(&pt);
        SetForegroundWindow(tray);
        int cmd = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, tray, nullptr);
        PostMessageW(tray, WM_NULL, 0, 0);
        DestroyMenu(m);
        if (cmd >= 100 && cmd - 100 < (int)items.size()) Launch({ items[cmd - 100].p.id }, false);
        else if (cmd == 1) ShowPicker();
        else if (cmd == 2) AddAccount();
        else if (cmd == 3) ShowSettings();
        else if (cmd == 4) Exit();
    }

    // ------------------------------------------------------------ refresh

    static void Merge(const std::vector<Profile>& list, bool statusKnown) {
        auto autoOpen = Settings::List(L"AutoOpen");
        std::set<std::wstring> autoSet; for (auto& a : autoOpen) autoSet.insert(Util::Lower(a));
        std::vector<Item> next;
        ULONGLONG now = GetTickCount64();
        for (size_t k = 0; k < list.size(); k++) {
            Item it;
            if (auto old = Find(list[k].id)) it = *old;
            it.p = list[k];
            if (!statusKnown && Find(list[k].id)) it.p.pid = Find(list[k].id)->p.pid;
            it.name = Core::DisplayName(it.p);
            it.letters = Core::BadgeLetter(it.name);
            it.color = it.p.isDefault ? RGB(217, 119, 87) : Core::ProfileColor(it.p.id);
            it.index = (int)k + 1;
            it.autoOpen = autoSet.count(Util::Lower(it.p.id)) > 0;
            if (it.launching && (it.p.pid || now - it.launchAt > 20000)) it.launching = false;
            next.push_back(it);
        }
        items.swap(next);
        WinShell::UpdateJumpList(Profiles());
        if (statusKnown) WinShell::SplitUpdate(Profiles(), SplitTaskbar());
        RedrawAll();
    }

    // Profile list and running state, off the UI thread. Calls coalesce.
    void Refresh() {
        if (refreshing) { refreshAgain = true; return; }
        refreshing = true;
        std::thread([] {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            auto list = new std::vector<Profile>(Core::ProfileList(true));
            CoUninitialize();
            if (!PostMessageW(tray, WM_REFRESHED, 0, (LPARAM)list)) delete list;
        }).detach();
    }

    // ------------------------------------------------------------ windows

    void ShowPicker(bool atSignIn) {
        Flyout::Hide();
        std::vector<std::wstring> autoOpen;
        if (atSignIn) for (auto& i : items) if (i.autoOpen) autoOpen.push_back(i.p.id);
        Picker::Show(atSignIn, autoOpen);
        Refresh();
    }

    void ClosePicker() {
        Picker::Close();
        if (!KeepInTray()) Exit();
    }

    void ShowSettings() { Flyout::Hide(); SettingsWin::Show(); Refresh(); }

    void ThemeChanged() {
        Gfx::ApplyTheme();
        WinShell::DarkMenus(Gfx::T.dark);
        Picker::Restyle(); Flyout::Restyle(); SettingsWin::Restyle(); Dialogs::RestyleAll();
    }

    void Exit() {
        WinShell::SplitShutdown();
        if (iconAdded) { NOTIFYICONDATAW d{ sizeof d }; d.hWnd = tray; d.uID = 1; Shell_NotifyIconW(NIM_DELETE, &d); iconAdded = false; }
        PostQuitMessage(0);
    }

    static HWND Owner(HWND w) { return w && IsWindowVisible(w) ? w : Picker::Visible() ? Picker::Handle() : nullptr; }

    // ------------------------------------------------------------ actions

    struct LaunchResult { std::vector<std::wstring> errors; };

    void Launch(const std::vector<std::wstring>& ids, bool fromPicker) {
        if (ids.empty()) return;
        for (auto& id : ids) if (auto it = Find(id)) if (!it->running()) { it->launching = true; it->launchAt = GetTickCount64(); }
        RedrawAll();
        std::thread([ids] {
            auto r = new LaunchResult();
            for (auto& id : ids) {
                try { Core::Start(id); }
                catch (const std::wstring& e) { r->errors.push_back(e); Core::Log(e); }
                catch (...) { r->errors.push_back(Util::Format(Tr(L"Couldn't open %s", L"Не удалось открыть %s"), Util::Q(id).c_str())); }
            }
            if (!PostMessageW(tray, WM_LAUNCHED, 0, (LPARAM)r)) delete r;
        }).detach();
        // A moment for the card to show it is starting, then out of the way.
        if (fromPicker && HideAfterLaunch()) SetTimer(tray, TimerHide, 450, nullptr);
    }

    void AddAccount() {
        std::thread([] {
            Profile* p = nullptr;
            try { p = new Profile(Core::AddAccount()); } catch (...) {}
            PostMessageW(tray, WM_ADDED, 0, (LPARAM)p);
        }).detach();
    }

    // Started from the message loop rather than right away: a menu that has just closed
    // hands focus back to its window a moment later, which would end the edit at once.
    void BeginRename(const std::wstring& id) { pendingRename = id; PostMessageW(tray, WM_RENAME, 0, 0); }

    void CommitRename(const std::wstring& id, const std::wstring& raw) {
        auto it = Find(id);
        if (!it) return;
        std::wstring name = raw;
        while (!name.empty() && name.back() == L' ') name.pop_back();
        while (!name.empty() && name.front() == L' ') name.erase(0, 1);
        if (name.empty() || name == it->name) return;
        if (it->p.isDefault && (name == L"Main" || name == L"Основной")) name = Core::DefaultId;   // back to its own name
        try {
            auto renamed = Core::Rename(it->p, name);
            it->p = renamed; it->name = Core::DisplayName(renamed); it->letters = Core::BadgeLetter(it->name);
            WinShell::UpdateJumpList(Profiles());
            RedrawAll();
        } catch (const std::wstring& e) { Dialogs::Info(Owner(nullptr), Tr(L"Couldn't rename", L"Не удалось переименовать"), e); }
    }

    void Delete(const std::wstring& id, HWND owner) {
        auto it = Find(id);
        if (!it || it->p.isDefault) return;
        auto name = it->name;
        if (!Dialogs::Ask(Owner(owner), Util::Format(Tr(L"Delete %s?", L"Удалить %s?"), Util::Q(name).c_str()),
                Tr(L"This signs the account out on this PC. Its profile folder, saved sign-in and shortcuts are deleted; other accounts stay",
                   L"Вы выйдете из этого аккаунта на этом компьютере. Папка профиля, сохранённый вход и ярлыки удалятся, остальные аккаунты останутся"),
                Tr(L"Delete", L"Да, удалить"), true)) return;
        it = Find(id);
        if (!it) return;
        try {
            Core::Delete(it->p);
            items.erase(items.begin() + (it - &items[0]));
            SaveAutoOpen();
            Refresh();
        } catch (const std::wstring& e) { Dialogs::Info(Owner(owner), Tr(L"Couldn't delete", L"Не удалось удалить"), e); }
    }

    void CopyChats(HWND owner, const std::vector<CodeSession>& picked, const std::wstring& targetId) {
        auto t = Find(targetId);
        if (!t) return;
        auto name = t->name; bool live = t->running();
        auto store = Core::CodeSessionStore(t->p);
        if (store.empty()) {
            Dialogs::Info(owner, Util::Format(Tr(L"Claude Code hasn't been used in %s yet", L"В %s ещё не открывали Claude Code"), Util::Q(name).c_str()),
                          Tr(L"Open this account, sign in and visit the Code tab once. Then try again", L"Откройте этот аккаунт, войдите и один раз зайдите на вкладку Code. Потом повторите перенос"));
            return;
        }
        if (live && !Dialogs::Ask(owner, Util::Format(Tr(L"%s is open", L"%s сейчас открыт"), Util::Q(name).c_str()),
                                  Tr(L"The copied chats appear once you close and reopen that Claude window", L"Скопированные чаты появятся, когда вы закроете и снова откроете это окно Claude"),
                                  Tr(L"Copy", L"Скопировать"))) return;
        int copied = 0, skipped = 0;
        try { for (auto& s : picked) { if (Core::CopyCodeSession(s, store)) copied++; else skipped++; } }
        catch (const std::wstring& e) { Dialogs::Info(owner, Tr(L"Couldn't move the chats", L"Перенос не удался"), e); return; }
        auto title = Tr(L"Copied: ", L"Скопировано: ") + Util::Count(copied, L"chat", L"chats", L"чат", L"чата", L"чатов");
        auto text = skipped ? Util::Format(Tr(L"%s already in %s", L"Ещё %s в %s"), Util::Count(skipped, L"chat was", L"chats were", L"чат уже был", L"чата уже были", L"чатов уже были").c_str(), Util::Q(name).c_str())
                            : Util::Format(Tr(L"They are now in %s too", L"Теперь они есть и в %s"), Util::Q(name).c_str());
        if (copied && !live) { if (Dialogs::Ask(owner, title, text, Util::Format(Tr(L"Open %s", L"Открыть %s"), Util::Q(name).c_str()), false, Tr(L"Later", L"Позже"))) Launch({ targetId }, true); }
        else Dialogs::Info(owner, title, text);
    }

    // The ⋯ menu of an account card. Native, so it has the system's look, animation and
    // keyboard behaviour; uxtheme's dark mode makes it follow the theme.
    void ShowMenu(const std::wstring& id, POINT at, HWND owner) {
        auto it = Find(id);
        if (!it) return;
        WinShell::DarkMenus(Gfx::T.dark);
        WinShell::AllowDark(owner, Gfx::T.dark);
        HMENU m = CreatePopupMenu();
        auto key = it->index <= 9 ? Util::Format(L"\t%d", it->index) : L"";
        AppendMenuW(m, MF_STRING, 1, ((it->running() ? Tr(L"Switch to window", L"Перейти к окну") : Tr(L"Open", L"Открыть")) + key).c_str());
        AppendMenuW(m, MF_STRING | (it->autoOpen ? MF_CHECKED : 0), 2, Tr(L"Open when Windows starts", L"Открывать при входе в Windows"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, 3, Tr(L"Rename\tF2", L"Переименовать\tF2"));
        AppendMenuW(m, MF_STRING, 4, Tr(L"Desktop shortcut", L"Ярлык на рабочий стол"));
        AppendMenuW(m, MF_STRING, 5, Tr(L"Move Code chats…", L"Перенести чаты Code…"));
        AppendMenuW(m, MF_STRING, 6, Tr(L"Show profile folder", L"Показать папку профиля"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING | (it->p.isDefault ? MF_GRAYED : 0), 7, Tr(L"Delete…\tDel", L"Удалить…\tDel"));
        SetMenuDefaultItem(m, 1, FALSE);
        int cmd = TrackPopupMenuEx(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, at.x, at.y, owner, nullptr);
        DestroyMenu(m);
        it = Find(id);
        if (!it) return;
        switch (cmd) {
        case 1: Launch({ id }, true); break;
        case 2: ToggleAutoOpen(id); break;
        case 3: BeginRename(id); break;
        case 4:
            Core::CreateProfileShortcut(it->p, Paths::Desktop());
            Notify(Tr(L"Shortcut created", L"Ярлык готов"), Util::Format(Tr(L"%s is on the desktop", L"%s на рабочем столе"), Util::Q(L"Claude - " + it->p.name).c_str()));
            break;
        case 5: Dialogs::Transfer(owner, id); break;
        case 6:
            if (Util::IsDir(it->p.path)) ShellExecuteW(nullptr, L"open", it->p.path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else Dialogs::Info(owner, Tr(L"No folder yet", L"Папки пока нет"), Tr(L"It appears when the account is opened for the first time", L"Она появится, когда аккаунт откроется в первый раз"));
            break;
        case 7: Delete(id, owner); break;
        }
    }

    // ------------------------------------------------------------ messages

    static LRESULT CALLBACK TrayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_TRAY:
            switch (LOWORD(l)) {
            // Version 4 also sends the raw button messages; reacting to those as well would
            // open and immediately close the flyout.
            case NIN_SELECT: case NIN_KEYSELECT: Flyout::Toggle(); return 0;
            case WM_CONTEXTMENU: TrayMenu(); return 0;
            }
            return 0;
        case WM_HOTKEY:
            if (w == HotkeyPicker) { if (Picker::Visible() && Picker::Active()) ClosePicker(); else ShowPicker(); }
            else if (w > HotkeyAccount && w <= HotkeyAccount + 9) {
                int d = (int)w - HotkeyAccount;
                for (auto& i : items) if (i.index == d) Launch({ i.p.id }, false);
            }
            return 0;
        case WM_REFRESHED: {
            auto list = (std::vector<Profile>*)l;
            refreshing = false;
            Merge(*list, true);
            delete list;
            if (refreshAgain) { refreshAgain = false; Refresh(); }
            return 0;
        }
        case WM_LAUNCHED: {
            auto r = (LaunchResult*)l;
            if (!r->errors.empty()) {
                for (auto& i : items) i.launching = false;
                if (Picker::Visible()) Dialogs::Info(Picker::Handle(), Tr(L"Couldn't open Claude", L"Не удалось открыть Claude"), r->errors[0]);
                else Notify(Tr(L"Couldn't open Claude", L"Не удалось открыть Claude"), r->errors[0], true);
            }
            delete r;
            Refresh();
            // The window takes a few seconds to appear; look again until it has.
            SetTimer(tray, TimerSoon, 2500, nullptr);
            return 0;
        }
        case WM_ADDED: {
            auto p = (Profile*)l;
            if (!p) { Dialogs::Info(Owner(nullptr), Tr(L"Couldn't add an account", L"Не удалось добавить аккаунт"), Tr(L"Details were written to ", L"Подробности записаны в ") + Paths::Log); return 0; }
            auto id = p->id;
            std::vector<Profile> list; for (auto& i : items) list.push_back(i.p);
            list.push_back(*p); delete p;
            Merge(list, false);
            if (auto it = Find(id)) { it->launching = true; it->launchAt = GetTickCount64(); }
            ShowPicker();
            Picker::NewAccountHint(id);
            BeginRename(id);
            Refresh();
            return 0;
        }
        case WM_RENAME:
            if (!pendingRename.empty()) { auto id = pendingRename; pendingRename.clear(); Picker::Rename(id); }
            return 0;
        case WM_SIGNAL:
            if (w == 0) ShowPicker();
            else if (w == 1) Exit();
            else if (w == 2) { ShowPicker(); AddAccount(); }
            return 0;
        case WM_TIMER:
            if (w == TimerRefresh) {
                bool busy = Picker::Visible() || Flyout::Visible() || SettingsWin::Visible();
                for (auto& i : items) busy |= i.launching;
                if (busy) Refresh();
            } else if (w == TimerSoon) {
                KillTimer(h, TimerSoon);
                bool pending = false; for (auto& i : items) pending |= i.launching;
                Refresh();
                if (pending) SetTimer(h, TimerSoon, 3000, nullptr);
            } else if (w == TimerHide) {
                KillTimer(h, TimerHide);
                if (Picker::Visible()) ClosePicker();
            }
            return 0;
        case WM_SETTINGCHANGE:
            if (l && wcscmp((const wchar_t*)l, L"ImmersiveColorSet") == 0) ThemeChanged();
            return 0;
        }
        if (m == taskbarCreated) { AddIcon(); return 0; }   // Explorer restarted
        return DefWindowProcW(h, m, w, l);
    }

    // Single-instance requests from other copies arrive as events, turned into messages here.
    static void Watch(HANDLE e, WPARAM which) {
        HANDLE wait;
        RegisterWaitForSingleObject(&wait, e, [](PVOID p, BOOLEAN) { PostMessageW(tray, WM_SIGNAL, (WPARAM)p, 0); }, (PVOID)which, INFINITE, WT_EXECUTEDEFAULT);
    }

    void Start(StartMode mode) {
        taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
        WNDCLASSEXW wc{ sizeof wc };
        wc.lpfnWndProc = TrayProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"ClaudeProfileSwitcherTray";
        RegisterClassExW(&wc);
        tray = CreateWindowExW(0, wc.lpszClassName, L"ClaudeProfileSwitcherTray", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
        smallIcon = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        AddIcon();
        RegisterPicker();
        RegisterAccounts();
        WinShell::DarkMenus(Gfx::T.dark);

        showEvt = CreateEventW(nullptr, FALSE, FALSE, Core::ShowEvent);
        exitEvt = CreateEventW(nullptr, FALSE, FALSE, Core::ExitEvent);
        addEvt = CreateEventW(nullptr, FALSE, FALSE, Core::AddEvent);
        // We own the single-instance mutex, so anything still pending was meant for a copy that is gone.
        ResetEvent(exitEvt); ResetEvent(addEvt);
        Watch(showEvt, 0); Watch(exitEvt, 1); Watch(addEvt, 2);

        // Names come up at once; running state and limits follow a moment later.
        Merge(Core::ProfileList(true), true);
        SetTimer(tray, TimerRefresh, 2000, nullptr);

        if (mode == StartMode::Window) ShowPicker();
        else if (mode == StartMode::Startup) {
            std::vector<std::wstring> autoIds; std::vector<std::wstring> names;
            for (auto& i : items) if (i.autoOpen) { autoIds.push_back(i.p.id); names.push_back(i.name); }
            if (AskAtStartup()) ShowPicker(true);
            else if (!autoIds.empty()) { Launch(autoIds, false); Notify(AppTitle(), Tr(L"Opening ", L"Открываю ") + Util::Names(names)); }
        }
    }
}

// ================================================================ hotkey text

namespace Hotkeys {
    static std::wstring KeyName(UINT vk) {
        if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::wstring(1, (wchar_t)vk);
        if (vk >= VK_F1 && vk <= VK_F24) return Util::Format(L"F%d", vk - VK_F1 + 1);
        switch (vk) {
        case VK_SPACE: return L"Space"; case VK_OEM_COMMA: return L","; case VK_OEM_PERIOD: return L".";
        case VK_OEM_3: return L"`"; case VK_OEM_MINUS: return L"-"; case VK_OEM_PLUS: return L"=";
        case VK_INSERT: return L"Insert"; case VK_HOME: return L"Home"; case VK_END: return L"End";
        case VK_PRIOR: return L"PageUp"; case VK_NEXT: return L"PageDown";
        }
        return Util::Format(L"0x%02X", vk);
    }

    std::wstring FromKeys(UINT mods, UINT vk) {
        std::wstring s;
        if (mods & MOD_CONTROL) s += L"Ctrl+";
        if (mods & MOD_ALT) s += L"Alt+";
        if (mods & MOD_SHIFT) s += L"Shift+";
        if (mods & MOD_WIN) s += L"Win+";
        return s + KeyName(vk);
    }

    bool Parse(const std::wstring& combo, UINT& mods, UINT& vk) {
        mods = 0; vk = 0;
        if (combo.empty()) return false;
        size_t s = 0;
        while (s <= combo.size()) {
            auto e = combo.find(L'+', s);
            auto part = combo.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s);
            if (part.empty() && e != std::wstring::npos) { part = L"+"; }   // never produced, but harmless
            auto lp = Util::Lower(part);
            if (lp == L"ctrl") mods |= MOD_CONTROL;
            else if (lp == L"alt") mods |= MOD_ALT;
            else if (lp == L"shift") mods |= MOD_SHIFT;
            else if (lp == L"win") mods |= MOD_WIN;
            else if (part.size() == 1) vk = (UINT)towupper(part[0]) == L',' ? VK_OEM_COMMA : part[0] == L'.' ? VK_OEM_PERIOD : (UINT)towupper(part[0]);
            else if (lp.size() > 1 && lp[0] == L'f' && iswdigit(lp[1])) vk = VK_F1 + _wtoi(lp.c_str() + 1) - 1;
            else if (lp == L"space") vk = VK_SPACE;
            else if (lp.rfind(L"0x", 0) == 0) vk = wcstoul(lp.c_str(), nullptr, 16);
            else if (lp == L"oemcomma") vk = VK_OEM_COMMA;   // written by the .NET version
            if (e == std::wstring::npos) break;
            s = e + 1;
        }
        return vk != 0;
    }

    std::wstring Pretty(const std::wstring& combo) {
        std::wstring out;
        for (wchar_t c : combo) { if (c == L'+') out += L" + "; else out += c; }
        return out;
    }
}
