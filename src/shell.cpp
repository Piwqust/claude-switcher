#include "shell.h"
#include "gfx.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <tlhelp32.h>
#include <cmath>
#include <set>

using namespace Gfx;

// ============================================================ icon files

#include "icons.inc"

    // ============================================================ shortcuts

    static void SaveLink(const std::wstring& lnk, const std::wstring& target, const std::wstring& args, const std::wstring& icon, const std::wstring& desc) {
        CreateDirectoryW(Util::Parent(lnk).c_str(), nullptr);
        CreateDirectoryW(Paths::Home.c_str(), nullptr);
        IShellLinkW* link = nullptr;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) return;
        link->SetPath(target.c_str()); link->SetArguments(args.c_str());
        link->SetIconLocation(icon.c_str(), 0); link->SetDescription(desc.c_str());
        link->SetWorkingDirectory(Paths::Home.c_str());
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&pf)))) { pf->Save(lnk.c_str(), TRUE); pf->Release(); }
        link->Release();
    }

    std::wstring CreateProfileShortcut(const Profile& p, const std::wstring& dir) {
        auto lnk = dir + L"\\Claude - " + p.name + L".lnk";
        auto desc = Util::Format(Tr(L"Open Claude with the %s account", L"Открыть Claude с аккаунтом %s"), Util::Q(DisplayName(p)).c_str());
        if (p.isDefault && App.kind == L"Msix") SaveLink(lnk, L"explorer.exe", L"shell:AppsFolder\\" + App.aumid, ProfileIconPath(p), desc);
        // Goes through the switcher so Claude's path is resolved at click time and a click
        // on an open account brings its window forward.
        else SaveLink(lnk, Paths::Launcher(), L"--launch \"" + p.id + L"\"", ProfileIconPath(p), desc);
        return lnk;
    }

    std::vector<std::wstring> RemoveShortcuts(const std::wstring& label) {
        std::vector<std::wstring> removed;
        for (auto& dir : { Paths::Desktop(), Paths::StartMenu() }) {
            auto lnk = dir + L"\\Claude - " + label + L".lnk";
            if (Util::Exists(lnk) && DeleteFileW(lnk.c_str())) removed.push_back(lnk);
        }
        return removed;
    }

    // ============================================================ autostart

    static const wchar_t* RunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    static const wchar_t* RunValue = L"Claude Profile Switcher";

    bool AutostartEnabled() {
        if (Paths::Demo) return Settings::Flag(L"Autostart", false);
        wchar_t v[MAX_PATH * 2]; DWORD n = sizeof v;
        return RegGetValueW(HKEY_CURRENT_USER, RunKey, RunValue, RRF_RT_REG_SZ, nullptr, v, &n) == ERROR_SUCCESS && v[0];
    }

    void SetAutostart(bool on) {
        if (Paths::Demo) { Settings::Set(L"Autostart", on); return; }
        HKEY k;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
            if (on) { auto cmd = L"\"" + Paths::Launcher() + L"\" --startup"; RegSetValueExW(k, RunValue, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * 2)); }
            else RegDeleteValueW(k, RunValue);
            RegCloseKey(k);
        }
        // The PowerShell version used a Startup folder shortcut; two entries would start two copies.
        DeleteFileW((Paths::Startup() + L"\\Claude Profile Switcher.lnk").c_str());
        Settings::Set(L"Autostart", on);
    }

    // ============================================================ claude:// links

    // Claude registers claude:// in HKCU\Software\Classes on every start, but Windows gives
    // the links to the Store package's own registration all the same, so a sign-in finished
    // in the browser came back to the main account. Windows lets only the person change that,
    // so the switcher just offers itself as a handler; Claude's own key is left alone.
    static const wchar_t* LinkProgId = L"ClaudeProfileSwitcher.Link";
    static const wchar_t* LinkCaps = L"Software\\ClaudeProfileSwitcher\\Capabilities";

    static void RegStr(const std::wstring& key, const wchar_t* name, const std::wstring& value) {
        RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, REG_SZ, value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    }

    void RegisterLinks() {
        if (Paths::Demo) return;
        auto exe = Paths::Launcher();
        auto cls = std::wstring(L"Software\\Classes\\") + LinkProgId;
        RegStr(cls, nullptr, L"URL:Claude");
        RegStr(cls + L"\\DefaultIcon", nullptr, L"\"" + exe + L"\",0");
        RegStr(cls + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\" --open-link \"%1\"");
        RegStr(LinkCaps, L"ApplicationName", RunValue);
        RegStr(LinkCaps, L"ApplicationDescription", Tr(L"Opens claude:// links in the Claude account you used last", L"Открывает ссылки claude:// в аккаунте Claude, которым вы пользовались последним"));
        RegStr(std::wstring(LinkCaps) + L"\\URLAssociations", L"claude", LinkProgId);
        RegStr(L"Software\\RegisteredApplications", RunValue, LinkCaps);
    }

    bool LinksRouted() {
        if (Paths::Demo) return false;
        IApplicationAssociationRegistration* r = nullptr;
        if (FAILED(CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&r)))) return false;
        LPWSTR prog = nullptr; bool ours = false;
        if (SUCCEEDED(r->QueryCurrentDefault(L"claude", AT_URLPROTOCOL, AL_EFFECTIVE, &prog))) { ours = _wcsicmp(prog, LinkProgId) == 0; CoTaskMemFree(prog); }
        r->Release();
        return ours;
    }

    void ChooseLinkHandler() {
        RegisterLinks();
        // Windows 11 opens straight at the switcher's link types; older builds ignore the query
        // and show Default apps, where CLAUDE is under the link types.
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=Claude%20Profile%20Switcher", nullptr, nullptr, SW_SHOWNORMAL);
    }

    // ============================================================ install

    // Closes a running switcher, whichever version: the .NET ones listen for the exit
    // event, the PowerShell one is a powershell.exe running ClaudeSwitcher.ps1.
    static void StopOthers() {
        if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, ExitEvent)) {
            SetEvent(e); CloseHandle(e);
            for (int i = 0; i < 50; i++) {
                HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, MutexName);
                if (!m) break;
                CloseHandle(m); Sleep(100);
            }
        }
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe{ sizeof pe };
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            if (_wcsicmp(pe.szExeFile, L"powershell.exe") != 0) continue;
            auto cmd = Util::Lower(Util::CommandLineOf(pe.th32ProcessID));
            auto f = cmd.find(L"-file ");
            // Only a PowerShell actually running the script, not any shell that mentions it.
            if (f == std::wstring::npos || cmd.find(L"\\claudeswitcher.ps1", f) == std::wstring::npos) continue;
            if (HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID)) { TerminateProcess(p, 0); CloseHandle(p); }
        }
        CloseHandle(snap);
    }

    void Install() {
        CreateDirectoryW(Paths::Root.c_str(), nullptr);
        CreateDirectoryW(Paths::Home.c_str(), nullptr);
        StopOthers();
        if (!Paths::Same(Paths::Self, Paths::InstalledExe)) {
            BOOL copied = FALSE;
            for (int i = 0; i < 20 && !copied; i++) { copied = CopyFileW(Paths::Self.c_str(), Paths::InstalledExe.c_str(), FALSE); if (!copied) Sleep(250); }
            if (!copied) throw Util::Format(Tr(L"Couldn't copy the program to %s (code %lu)", L"Не удалось скопировать программу в %s (код %lu)"), Paths::Home.c_str(), GetLastError());
            // Drop the "downloaded from the internet" mark so Windows does not warn on start.
            DeleteFileW((Paths::InstalledExe + L":Zone.Identifier").c_str());
        }
        auto exe = Paths::InstalledExe;
        auto about = Tr(L"Switch between Claude accounts", L"Переключение между аккаунтами Claude");
        SaveLink(Paths::Desktop() + L"\\Claude Profile Switcher.lnk", exe, L"", exe, about);
        SaveLink(Paths::StartMenu() + L"\\Claude Profile Switcher.lnk", exe, L"", exe, about);
        SaveLink(Paths::StartMenu() + L"\\Claude - Add account.lnk", exe, L"--add-account", AddAccountIconPath(),
                 Tr(L"Sign in to another Claude account in a separate window", L"Войти в ещё один аккаунт Claude в отдельном окне"));
        // Existing profile shortcuts may still point at an older version.
        for (auto& p : ProfileList(false))
            for (auto& dir : { Paths::Desktop(), Paths::StartMenu() })
                if (Util::Exists(dir + L"\\Claude - " + p.name + L".lnk")) CreateProfileShortcut(p, dir);
        SetAutostart(Settings::Flag(L"Autostart", true));
        RegisterLinks();
        // Explorer caches shortcut icons; tell it they changed.
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
}

// ============================================================ jump list

namespace WinShell {
    static IShellLinkW* Task(const std::wstring& title, const std::wstring& args, const std::wstring& icon, const std::wstring& desc) {
        IShellLinkW* l = nullptr;
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&l));
        if (!l) return nullptr;
        l->SetPath(Paths::Launcher().c_str()); l->SetArguments(args.c_str());
        l->SetIconLocation(icon.c_str(), 0); l->SetDescription(desc.c_str());
        IPropertyStore* ps = nullptr;
        if (SUCCEEDED(l->QueryInterface(IID_PPV_ARGS(&ps)))) {
            PROPVARIANT v; InitPropVariantFromString(title.c_str(), &v);
            ps->SetValue(PKEY_Title, v); ps->Commit(); PropVariantClear(&v); ps->Release();
        }
        return l;
    }

    // Right-click on the pinned taskbar button lists the accounts.
    void UpdateJumpList(const std::vector<Profile>& profiles) {
        if (Paths::Demo) return;
        static std::wstring signature;
        std::wstring sig = Paths::Launcher() + (L10n::ru ? L"|ru" : L"|en");
        for (auto& p : profiles) sig += L"|" + p.id + L"=" + p.name;
        if (sig == signature) return;
        signature = sig;
        ICustomDestinationList* list = nullptr;
        if (FAILED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&list)))) return;
        UINT max; IObjectArray* removed = nullptr;
        if (SUCCEEDED(list->BeginList(&max, IID_PPV_ARGS(&removed)))) {
            IObjectCollection* accounts = nullptr, * tasks = nullptr;
            CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&accounts));
            CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tasks));
            for (auto& p : profiles) {
                auto name = Core::DisplayName(p);
                if (auto t = Task(name, L"--launch \"" + p.id + L"\"", Core::ProfileIconPath(p), Util::Format(Tr(L"Open Claude with the %s account", L"Открыть Claude с аккаунтом %s"), Util::Q(name).c_str()))) { accounts->AddObject(t); t->Release(); }
            }
            if (auto t = Task(Tr(L"Add account", L"Добавить аккаунт"), L"--add-account", Core::AddAccountIconPath(), Tr(L"Sign in to another Claude account", L"Войти в ещё один аккаунт Claude"))) { tasks->AddObject(t); t->Release(); }
            IObjectArray* arr = nullptr;
            if (SUCCEEDED(accounts->QueryInterface(IID_PPV_ARGS(&arr)))) { list->AppendCategory(Tr(L"Accounts", L"Аккаунты"), arr); arr->Release(); }
            if (SUCCEEDED(tasks->QueryInterface(IID_PPV_ARGS(&arr)))) { list->AddUserTasks(arr); arr->Release(); }
            list->CommitList();
            accounts->Release(); tasks->Release(); if (removed) removed->Release();
        }
        list->Release();
    }

    // ============================================================ dark menus

    // uxtheme's ordinal exports are how Explorer and every Windows 11 app get dark native
    // menus; they are undocumented but stable since 1903.
    void DarkMenus(bool dark) {
        static HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!ux) return;
        typedef int(WINAPI * SetMode)(int);
        typedef void(WINAPI * Flush)();
        auto set = (SetMode)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        auto flush = (Flush)GetProcAddress(ux, MAKEINTRESOURCEA(136));
        if (set) set(dark ? 2 /* ForceDark */ : 3 /* ForceLight */);
        if (flush) flush();
    }

    void AllowDark(HWND h, bool dark) {
        static HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        typedef bool(WINAPI * Allow)(HWND, bool);
        if (auto a = ux ? (Allow)GetProcAddress(ux, MAKEINTRESOURCEA(133)) : nullptr) a(h, dark);
    }

    // ============================================================ taskbar split

    /*
        Every Claude instance carries the same app identity, so Windows groups them under
        one taskbar button. A window's own System.AppUserModel.ID overrides that, and the
        shell accepts it from another process: tested, the taskbar regroups at once.
        Default keeps Claude's identity, so it stays with a pinned Claude. Turning the
        feature off puts every window back. Nothing inside Claude is changed.
    */
    struct Target { std::wstring id, name, icon; };
    struct Applied { std::wstring profile; HICON oldBig = nullptr, oldSmall = nullptr; };
    static bool splitOn = false;
    static HWINEVENTHOOK hook = nullptr;
    static std::map<DWORD, std::wstring> pidProfile;        // L"" = not an account window
    static std::map<HWND, Applied> applied;
    static std::map<std::wstring, Target> targets;
    static std::map<std::wstring, std::pair<HICON, HICON>> iconHandles;

    static bool SetProps(HWND h, const std::vector<std::pair<PROPERTYKEY, std::wstring>>& values) {
        IPropertyStore* ps = nullptr;
        if (FAILED(SHGetPropertyStoreForWindow(h, IID_PPV_ARGS(&ps)))) return false;
        bool ok = true;
        for (auto& kv : values) {
            PROPVARIANT v; PropVariantInit(&v);
            if (!kv.second.empty()) InitPropVariantFromString(kv.second.c_str(), &v);
            if (FAILED(ps->SetValue(kv.first, v))) ok = false;
            PropVariantClear(&v);
        }
        ok = ok && SUCCEEDED(ps->Commit());
        ps->Release();
        return ok;
    }

    static std::wstring AppId(const std::wstring& id) {
        // App ids allow no spaces; the hash keeps "Work" and "work." apart.
        std::wstring safe; UINT hash = 2166136261u;
        for (wchar_t c : Util::Lower(id)) hash = (hash ^ c) * 16777619u;
        for (wchar_t c : id) safe += iswalnum(c) && c < 128 ? c : L'_';
        return Util::Format(L"ClaudeProfileSwitcher.Account.%s.%08x", safe.c_str(), hash);
    }

    static LRESULT Send(HWND h, UINT m, WPARAM w, LPARAM l) { DWORD_PTR r = 0; SendMessageTimeoutW(h, m, w, l, SMTO_ABORTIFHUNG, 500, &r); return (LRESULT)r; }

    static void Apply(HWND h, const std::wstring& profile) {
        auto t = targets.find(profile);
        if (t == targets.end()) return;
        auto a = applied.find(h);
        if (a != applied.end() && a->second.profile == profile) return;
        // Relaunch properties first: pinning the separate button reopens this account.
        if (!SetProps(h, { { PKEY_AppUserModel_RelaunchCommand, L"\"" + Paths::Launcher() + L"\" --launch \"" + profile + L"\"" },
                           { PKEY_AppUserModel_RelaunchIconResource, t->second.icon + L",0" },
                           { PKEY_AppUserModel_RelaunchDisplayNameResource, L"Claude — " + t->second.name },
                           { PKEY_AppUserModel_ID, AppId(profile) } })) return;
        Applied rec = a != applied.end() ? a->second : Applied{};
        if (a == applied.end()) { rec.oldBig = (HICON)Send(h, WM_GETICON, ICON_BIG, 0); rec.oldSmall = (HICON)Send(h, WM_GETICON, ICON_SMALL, 0); }
        rec.profile = profile;
        applied[h] = rec;
        auto& icons = iconHandles[t->second.icon];
        if (!icons.first) {
            icons.first = (HICON)LoadImageW(nullptr, t->second.icon.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
            icons.second = (HICON)LoadImageW(nullptr, t->second.icon.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
        }
        if (icons.first) Send(h, WM_SETICON, ICON_BIG, (LPARAM)icons.first);
        if (icons.second) Send(h, WM_SETICON, ICON_SMALL, (LPARAM)icons.second);
    }

    static void Restore(HWND h, const Applied& a) {
        if (!IsWindow(h)) return;
        SetProps(h, { { PKEY_AppUserModel_RelaunchCommand, L"" }, { PKEY_AppUserModel_RelaunchIconResource, L"" },
                      { PKEY_AppUserModel_RelaunchDisplayNameResource, L"" },
                      { PKEY_AppUserModel_ID, Core::App.kind == L"Msix" ? Core::App.aumid : L"" } });
        if (a.oldBig) Send(h, WM_SETICON, ICON_BIG, (LPARAM)a.oldBig);
        if (a.oldSmall) Send(h, WM_SETICON, ICON_SMALL, (LPARAM)a.oldSmall);
    }

    static bool IsAppWindow(HWND h) {
        return IsWindowVisible(h) && GetAncestor(h, GA_ROOT) == h && !GetWindow(h, GW_OWNER) && !(GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW);
    }

    // Which account a process runs, resolved once per process.
    static std::wstring ProfileForPid(DWORD pid) {
        auto it = pidProfile.find(pid);
        if (it != pidProfile.end()) return it->second;
        if (pidProfile.size() > 2000) pidProfile.clear();
        std::wstring result;
        auto image = Util::ImageOf(pid);
        if (_wcsicmp(Util::Leaf(image).c_str(), L"claude.exe") == 0) {
            auto cmd = Util::CommandLineOf(pid);
            auto i = cmd.find(L"--user-data-dir=");
            if (cmd.find(L"--type=") == std::wstring::npos && i != std::wstring::npos) {
                i += 16; std::wstring dir;
                if (cmd[i] == L'"') dir = cmd.substr(i + 1, cmd.find(L'"', i + 1) - i - 1); else dir = cmd.substr(i, cmd.find(L' ', i) - i);
                while (!dir.empty() && dir.back() == L'\\') dir.pop_back();
                if (Paths::Same(Util::Parent(dir), Paths::Root)) result = Util::Leaf(dir);
            }
        }
        pidProfile[pid] = result;
        return result;
    }

    static void CALLBACK OnShow(HWINEVENTHOOK, DWORD, HWND h, LONG obj, LONG child, DWORD, DWORD) {
        if (obj != OBJID_WINDOW || child != 0 || !h || !IsAppWindow(h)) return;
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        auto profile = ProfileForPid(pid);
        if (!profile.empty()) Apply(h, profile);
    }

    void SplitUpdate(const std::vector<Profile>& profiles, bool on) {
        if (Paths::Demo) return;
        if (!on) {
            if (!splitOn) return;
            splitOn = false;
            if (hook) { UnhookWinEvent(hook); hook = nullptr; }
            for (auto& kv : applied) Restore(kv.first, kv.second);
            applied.clear(); pidProfile.clear();
            return;
        }
        if (!splitOn) {
            splitOn = true;
            // Out of context: no code goes into other processes; events arrive on this thread.
            hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, OnShow, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        }
        targets.clear();
        for (auto& p : profiles) {
            if (p.isDefault) { if (p.pid) pidProfile[p.pid] = L""; continue; }
            targets[p.id] = { p.id, Core::DisplayName(p), Core::ProfileIconPath(p) };
            if (p.pid) {
                pidProfile[p.pid] = p.id;
                for (HWND h : Util::AppWindowsOf(p.pid)) Apply(h, p.id);
            }
        }
    }

    // Our icon handles die with this process, so the windows get theirs back first.
    void SplitShutdown() { SplitUpdate({}, false); }
}
