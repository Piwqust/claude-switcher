#include "core.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <algorithm>
#include <cwctype>
#include <cstdarg>
#include <set>

// ================================================================ paths

namespace Paths {
    std::wstring Root, Home, IconDir, InstalledExe, SettingsFile, Log, Self;
    bool Demo = false;

    static std::wstring Known(REFKNOWNFOLDERID id) {
        PWSTR p = nullptr; std::wstring out;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = p;
        CoTaskMemFree(p);
        return out;
    }

    void Init() {
        Root = Known(FOLDERID_LocalAppData) + L"\\ClaudeProfiles";
        // CPS_DEMO=<folder> runs a separate switcher over stand-in accounts in that folder,
        // for screenshots and trying things out. It never starts Claude and leaves shortcuts,
        // autostart, hotkeys, the jump list and the real switcher alone.
        wchar_t demo[MAX_PATH * 2];
        if (GetEnvironmentVariableW(L"CPS_DEMO", demo, ARRAYSIZE(demo))) {
            Demo = true; Root = demo;
            Core::MutexName = L"Local\\ClaudeProfileSwitcherDemo";
            Core::ShowEvent = L"Local\\ClaudeProfileSwitcherDemo.Show";
            Core::ExitEvent = L"Local\\ClaudeProfileSwitcherDemo.Exit";
            Core::AddEvent = L"Local\\ClaudeProfileSwitcherDemo.Add";
        }
        Home = Root + L"\\.switcher";
        IconDir = Home + L"\\icons";
        InstalledExe = Home + L"\\ClaudeProfileSwitcher.exe";
        SettingsFile = Root + L"\\settings.json";
        Log = Root + L"\\switcher-error.log";
        wchar_t buf[MAX_PATH * 2]; GetModuleFileNameW(nullptr, buf, ARRAYSIZE(buf)); Self = buf;
    }

    // Shortcuts point at the installed copy, so moving the download breaks nothing.
    std::wstring Launcher() { return Util::Exists(InstalledExe) ? InstalledExe : Self; }
    std::wstring Desktop() { return Known(FOLDERID_Desktop); }
    std::wstring StartMenu() { return Known(FOLDERID_Programs); }
    std::wstring Startup() { return Known(FOLDERID_Startup); }

    bool Same(const std::wstring& a, const std::wstring& b) {
        wchar_t fa[MAX_PATH * 2], fb[MAX_PATH * 2];
        GetFullPathNameW(a.c_str(), ARRAYSIZE(fa), fa, nullptr);
        GetFullPathNameW(b.c_str(), ARRAYSIZE(fb), fb, nullptr);
        std::wstring x = fa, y = fb;
        while (!x.empty() && x.back() == L'\\') x.pop_back();
        while (!y.empty() && y.back() == L'\\') y.pop_back();
        return _wcsicmp(x.c_str(), y.c_str()) == 0;
    }
}

// ================================================================ language

namespace L10n {
    bool ru = false;

    static bool SystemRussian() { return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN; }

    static void Apply(const std::wstring& choice) {
        wchar_t forced[8] = {};
        GetEnvironmentVariableW(L"CPS_LANG", forced, ARRAYSIZE(forced));
        if (!wcscmp(forced, L"en") || !wcscmp(forced, L"ru")) { ru = !wcscmp(forced, L"ru"); return; }
        ru = choice == L"ru" || (choice != L"en" && SystemRussian());
    }

    std::wstring Choice() { auto c = Settings::Str(L"Language", L"auto"); return c == L"en" || c == L"ru" ? c : L"auto"; }
    void Init() { Apply(Choice()); }
    void Choose(const std::wstring& choice) { Settings::Set(L"Language", choice); Apply(choice); }
}

// ================================================================ util

namespace Util {
    std::wstring ReadText(const std::wstring& path, bool shared) {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | (shared ? FILE_SHARE_WRITE | FILE_SHARE_DELETE : 0),
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return L"";
        LARGE_INTEGER size; GetFileSizeEx(h, &size);
        std::string bytes((size_t)std::min<LONGLONG>(size.QuadPart, 64ll << 20), '\0');
        DWORD got = 0;
        if (!bytes.empty()) ReadFile(h, &bytes[0], (DWORD)bytes.size(), &got, nullptr);
        CloseHandle(h);
        bytes.resize(got);
        size_t skip = bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF && (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF ? 3 : 0;
        int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + skip, (int)(bytes.size() - skip), nullptr, 0);
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, bytes.data() + skip, (int)(bytes.size() - skip), &out[0], n);
        return out;
    }

    bool WriteText(const std::wstring& path, const std::wstring& text, bool bom) {
        int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        std::string bytes = bom ? "\xEF\xBB\xBF" : "";
        size_t at = bytes.size(); bytes.resize(at + n);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &bytes[at], n, nullptr, nullptr);
        // Written beside and swapped in, so a crash never leaves half a settings file.
        std::wstring tmp = path + L".tmp";
        HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        DWORD wrote = 0; WriteFile(h, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr); CloseHandle(h);
        return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    }

    bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
    bool IsDir(const std::wstring& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
    std::wstring Parent(const std::wstring& p) { auto i = p.find_last_of(L"\\/"); return i == std::wstring::npos ? L"" : p.substr(0, i); }
    std::wstring Leaf(const std::wstring& p) { auto i = p.find_last_of(L"\\/"); return i == std::wstring::npos ? p : p.substr(i + 1); }
    std::wstring Join(const std::wstring& a, const std::wstring& b) { return a.empty() || a.back() == L'\\' ? a + b : a + L"\\" + b; }
    std::wstring Lower(std::wstring s) { CharLowerBuffW(&s[0], (DWORD)s.size()); return s; }

    std::wstring Format(const wchar_t* fmt, ...) {
        wchar_t buf[2048]; va_list a; va_start(a, fmt); _vsnwprintf_s(buf, _TRUNCATE, fmt, a); va_end(a); return buf;
    }

    ULONGLONG FileTime(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA d;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d)) return 0;
        return ((ULONGLONG)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    }

    // The command line of another process of this user, without WMI: a few microseconds
    // instead of a few hundred milliseconds.
    std::wstring CommandLineOf(DWORD pid) {
        typedef NTSTATUS(NTAPI* Fn)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        static Fn query = (Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return L"";
        std::wstring out;
        ULONG len = 0;
        query(h, 60 /* ProcessCommandLineInformation */, nullptr, 0, &len);
        if (len) {
            std::vector<BYTE> buf(len);
            if (query(h, 60, buf.data(), len, &len) >= 0) {
                auto us = (UNICODE_STRING*)buf.data();
                out.assign(us->Buffer, us->Length / sizeof(wchar_t));
            }
        }
        CloseHandle(h);
        return out;
    }

    std::wstring ImageOf(DWORD pid) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h) return L"";
        wchar_t buf[MAX_PATH * 2]; DWORD n = ARRAYSIZE(buf);
        std::wstring out = QueryFullProcessImageNameW(h, 0, buf, &n) ? buf : L"";
        CloseHandle(h);
        return out;
    }

    std::vector<HWND> AppWindowsOf(DWORD pid) {
        struct Ctx { DWORD pid; std::vector<HWND> out; } ctx{ pid };
        EnumWindows([](HWND h, LPARAM l) -> BOOL {
            auto c = (Ctx*)l; DWORD p = 0; GetWindowThreadProcessId(h, &p);
            if (p == c->pid && IsWindowVisible(h) && !GetWindow(h, GW_OWNER) && !(GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW))
                c->out.push_back(h);
            return TRUE;
        }, (LPARAM)&ctx);
        return ctx.out;
    }

    std::wstring Plural(int n, const wchar_t* one, const wchar_t* few, const wchar_t* many) {
        int m10 = n % 10, m100 = n % 100;
        const wchar_t* w = m10 == 1 && m100 != 11 ? one : (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) ? few : many;
        return Format(L"%d %s", n, w);
    }

    std::wstring Count(int n, const wchar_t* enOne, const wchar_t* enMany, const wchar_t* ruOne, const wchar_t* ruFew, const wchar_t* ruMany) {
        return L10n::ru ? Plural(n, ruOne, ruFew, ruMany) : Format(L"%d %s", n, n == 1 ? enOne : enMany);
    }

    std::wstring Q(const std::wstring& s) { return L10n::ru ? L"«" + s + L"»" : L"“" + s + L"”"; }

    static const wchar_t* Months[] = { L"января", L"февраля", L"марта", L"апреля", L"мая", L"июня", L"июля",
                                       L"августа", L"сентября", L"октября", L"ноября", L"декабря" };
    static const wchar_t* MonthsEn[] = { L"January", L"February", L"March", L"April", L"May", L"June", L"July",
                                         L"August", L"September", L"October", L"November", L"December" };

    // 10:09 in Russian; in English the user's own clock format (10:09 AM, or 10:09).
    std::wstring Clock(const SYSTEMTIME& t) {
        if (L10n::ru) return Format(L"%d:%02d", t.wHour, t.wMinute);
        wchar_t b[64];
        if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &t, nullptr, b, ARRAYSIZE(b))) return b;
        return Format(L"%d:%02d", t.wHour, t.wMinute);
    }

    std::wstring When(ULONGLONG ft) {
        if (!ft) return Tr(L"Not opened yet", L"Ещё не открывался");
        FILETIME f{ (DWORD)ft, (DWORD)(ft >> 32) }; SYSTEMTIME u, t, now;
        FileTimeToSystemTime(&f, &u); SystemTimeToTzSpecificLocalTime(nullptr, &u, &t);
        GetLocalTime(&now);
        auto day = [](const SYSTEMTIME& s) { FILETIME x; SYSTEMTIME d = s; d.wHour = d.wMinute = d.wSecond = d.wMilliseconds = 0; SystemTimeToFileTime(&d, &x); return ((ULONGLONG)x.dwHighDateTime << 32 | x.dwLowDateTime) / 864000000000ull; };
        auto diff = (long long)day(now) - (long long)day(t);
        if (diff == 0) return Tr(L"Today, ", L"Сегодня, ") + Clock(t);
        if (diff == 1) return Tr(L"Yesterday, ", L"Вчера, ") + Clock(t);
        if (!L10n::ru) return t.wYear == now.wYear ? Format(L"%s %d", MonthsEn[t.wMonth - 1], t.wDay) : Format(L"%s %d, %d", MonthsEn[t.wMonth - 1], t.wDay, t.wYear);
        if (t.wYear == now.wYear) return Format(L"%d %s", t.wDay, Months[t.wMonth - 1]);
        return Format(L"%d %s %d", t.wDay, Months[t.wMonth - 1], t.wYear);
    }

    // “Work”, “Work” and “Personal”, or "3 accounts".
    std::wstring Names(const std::vector<std::wstring>& n) {
        if (n.size() == 1) return Q(n[0]);
        if (n.size() == 2) return Q(n[0]) + Tr(L" and ", L" и ") + Q(n[1]);
        return Count((int)n.size(), L"account", L"accounts", L"аккаунт", L"аккаунта", L"аккаунтов");
    }
}

// ================================================================ settings

namespace Settings {
    static CRITICAL_SECTION* Lock() {
        static CRITICAL_SECTION cs; static bool init = (InitializeCriticalSection(&cs), true); (void)init; return &cs;
    }

    json::Value Load() {
        EnterCriticalSection(Lock());
        json::Value v;
        auto text = Util::ReadText(Paths::SettingsFile);
        if (text.empty() || !json::Parser::Parse(text, v) || v.type != json::Value::Object) v = json::Value::MakeObject();
        LeaveCriticalSection(Lock());
        return v;
    }

    void Save(const json::Value& v) {
        EnterCriticalSection(Lock());
        CreateDirectoryW(Paths::Root.c_str(), nullptr);
        std::wstring out; json::Write(out, v);
        // With a BOM, so the PowerShell version still reads non-ASCII labels correctly.
        Util::WriteText(Paths::SettingsFile, out, true);
        LeaveCriticalSection(Lock());
    }

    void Set(const std::wstring& key, json::Value value) {
        EnterCriticalSection(Lock());
        auto v = Load(); v[key] = std::move(value); Save(v);
        LeaveCriticalSection(Lock());
    }

    bool Flag(const std::wstring& key, bool def) { return Load().Flag(key, def); }
    int Int(const std::wstring& key, int def) { return (int)Load().Num(key, def); }
    std::wstring Str(const std::wstring& key, const std::wstring& def) { return Load().Str(key, def); }
    bool Has(const std::wstring& key) { return Load().Find(key) != nullptr; }

    std::vector<std::wstring> List(const std::wstring& key) {
        std::vector<std::wstring> out;
        auto v = Load(); auto a = v.Find(key);
        if (a && a->type == json::Value::Array) for (auto& e : a->arr) if (e.type == json::Value::String) out.push_back(e.s);
        return out;
    }

    void SetList(const std::wstring& key, const std::vector<std::wstring>& items) {
        auto a = json::Value::MakeArray();
        for (auto& s : items) a.arr.emplace_back(s);
        Set(key, a);
    }
}

// ================================================================ discovery

namespace Core {
    ClaudeInstall App;
    const wchar_t* DefaultId = L"Default";
    const wchar_t* MutexName = L"Local\\ClaudeProfileSwitcherExe";
    const wchar_t* ShowEvent = L"Local\\ClaudeProfileSwitcherExe.Show";
    const wchar_t* ExitEvent = L"Local\\ClaudeProfileSwitcherExe.Exit";
    const wchar_t* AddEvent = L"Local\\ClaudeProfileSwitcherExe.Add";

    static std::wstring Env(const wchar_t* name) { wchar_t b[MAX_PATH * 2]; DWORD n = GetEnvironmentVariableW(name, b, ARRAYSIZE(b)); return n ? b : L""; }
    static std::wstring ClassicData() { return Env(L"APPDATA") + L"\\Claude"; }

    static std::wstring FindExe(const std::wstring& dir, int depth) {
        auto direct = Util::Join(dir, L"Claude.exe");
        if (Util::Exists(direct)) return direct;
        if (depth <= 0) return L"";
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(Util::Join(dir, L"*").c_str(), &fd);
        std::wstring best; ULONGLONG bestTime = 0;
        if (h == INVALID_HANDLE_VALUE) return L"";
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            auto hit = FindExe(Util::Join(dir, fd.cFileName), depth - 1);
            if (!hit.empty() && Util::FileTime(hit) >= bestTime) { best = hit; bestTime = Util::FileTime(hit); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return best;
    }

    // Older Store builds were redirected into the package container; current ones write
    // to the real %APPDATA%\Claude. Whichever exists is where Default lives.
    static std::wstring MsixData(const std::wstring& family) {
        auto packaged = Env(L"LOCALAPPDATA") + L"\\Packages\\" + family + L"\\LocalCache\\Roaming\\Claude";
        if (Util::IsDir(packaged)) return packaged;
        return Util::IsDir(ClassicData()) ? ClassicData() : packaged;
    }

    static bool FindMsix(ClaudeInstall& out) {
        // The per-user package repository lists installed packages and their folders,
        // which is what Get-AppxPackage reads.
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages",
                          0, KEY_READ, &k) != ERROR_SUCCESS) return false;
        bool found = false; ULONGLONG bestVer = 0;
        wchar_t name[512];
        for (DWORD i = 0;; i++) {
            DWORD n = ARRAYSIZE(name);
            if (RegEnumKeyExW(k, i, name, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            std::wstring full = name;
            if (_wcsnicmp(full.c_str(), L"Claude_", 7) != 0) continue;
            // Claude_<version>_<arch>_<resource>_<publisher>
            std::vector<std::wstring> parts; size_t s = 0, p;
            while ((p = full.find(L'_', s)) != std::wstring::npos) { parts.push_back(full.substr(s, p - s)); s = p + 1; }
            parts.push_back(full.substr(s));
            if (parts.size() < 5) continue;
            int a = 0, b = 0, c = 0, d = 0; swscanf_s(parts[1].c_str(), L"%d.%d.%d.%d", &a, &b, &c, &d);
            ULONGLONG ver = ((ULONGLONG)a << 48) | ((ULONGLONG)b << 32) | ((ULONGLONG)c << 16) | (ULONGLONG)d;
            wchar_t root[MAX_PATH * 2] = {}; DWORD rl = sizeof(root);
            if (RegGetValueW(k, name, L"PackageRootFolder", RRF_RT_REG_SZ, nullptr, root, &rl) != ERROR_SUCCESS)
                wcscpy_s(root, (Env(L"ProgramFiles") + L"\\WindowsApps\\" + full).c_str());
            auto exe = Util::Join(root, L"app\\Claude.exe");
            if (!Util::Exists(exe)) exe = FindExe(root, 2);
            if (exe.empty() || (found && ver <= bestVer)) continue;
            auto family = parts[0] + L"_" + parts.back();
            out = { L"Msix", exe, parts[1], family + L"!Claude", MsixData(family) };
            found = true; bestVer = ver;
        }
        RegCloseKey(k);
        return found;
    }

    static bool FindClassic(ClaudeInstall& out) {
        std::vector<std::wstring> dirs;
        for (HKEY hive : { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE })
        for (auto view : { L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall" }) {
            HKEY k; if (RegOpenKeyExW(hive, view, 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
            wchar_t sub[256];
            for (DWORD i = 0;; i++) {
                DWORD n = ARRAYSIZE(sub);
                if (RegEnumKeyExW(k, i, sub, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                wchar_t dn[512] = {}, loc[MAX_PATH * 2] = {}; DWORD a = sizeof(dn), b = sizeof(loc);
                RegGetValueW(k, sub, L"DisplayName", RRF_RT_REG_SZ, nullptr, dn, &a);
                RegGetValueW(k, sub, L"InstallLocation", RRF_RT_REG_SZ, nullptr, loc, &b);
                if (wcsstr(dn, L"Claude") && loc[0]) dirs.push_back(loc);
            }
            RegCloseKey(k);
        }
        dirs.push_back(Env(L"LOCALAPPDATA") + L"\\AnthropicClaude");
        dirs.push_back(Env(L"LOCALAPPDATA") + L"\\Programs\\Claude");
        dirs.push_back(Env(L"ProgramFiles") + L"\\Claude");
        for (auto& d : dirs) {
            if (!Util::IsDir(d)) continue;
            auto exe = FindExe(d, 2);
            if (!exe.empty()) { out = { L"Classic", exe, L"unknown", L"", ClassicData() }; return true; }
        }
        return false;
    }

    static void Detect(bool allowCache);
    void Init(bool allowCache) {
        Detect(allowCache);
        if (Paths::Demo) App.defaultData = Paths::Root + L"\\.default";
    }

    static void Detect(bool allowCache) {
        auto saved = Settings::Str(L"ClaudePath");
        if (!saved.empty() && Util::Exists(saved)) { App = { L"Classic", saved, L"unknown", L"", ClassicData() }; return; }
        if (allowCache) {
            // Shortcut clicks pay for discovery every time, so they may use the last result
            // as long as that versioned path still exists.
            auto all = Settings::Load(); auto c = all.Find(L"InstallCache");
            if (c && Util::Exists(c->Str(L"Exe"))) {
                App = { c->Str(L"Kind", L"Classic"), c->Str(L"Exe"), c->Str(L"Version", L"unknown"), c->Str(L"AppUserModelId"), c->Str(L"DefaultProfilePath", ClassicData()) };
                return;
            }
        }
        if (!FindMsix(App) && !FindClassic(App)) throw NotFound();
        auto c = json::Value::MakeObject();
        c[L"Kind"] = App.kind; c[L"Exe"] = App.exe; c[L"Version"] = App.version;
        c[L"AppUserModelId"] = App.aumid; c[L"DefaultProfilePath"] = App.defaultData;
        Settings::Set(L"InstallCache", c);
    }

    // ============================================================ profiles

    std::wstring ProfilePath(const std::wstring& id) { return id == DefaultId ? App.defaultData : Paths::Root + L"\\" + id; }

    // Is this the desktop app's own executable, whichever version of it?
    static bool IsDesktopExe(const std::wstring& image) {
        auto img = Util::Lower(image), exe = Util::Lower(App.exe);
        if (App.kind == L"Msix") {
            // ...\WindowsApps\Claude_<ver>_x64__<publisher>\app\Claude.exe; a window still on the
            // previous build after an update counts too.
            auto pkg = Util::Leaf(Util::Parent(Util::Parent(exe)));
            auto pub = pkg.substr(pkg.rfind(L"__") == std::wstring::npos ? pkg.size() : pkg.rfind(L"__"));
            auto ipkg = Util::Leaf(Util::Parent(Util::Parent(img)));
            return Util::Parent(Util::Parent(Util::Parent(img))) == Util::Parent(Util::Parent(Util::Parent(exe))) &&
                   ipkg.rfind(L"claude_", 0) == 0 && !pub.empty() && ipkg.size() >= pub.size() &&
                   ipkg.compare(ipkg.size() - pub.size(), pub.size(), pub) == 0;
        }
        auto dir = Util::Parent(exe);
        if (Util::Leaf(dir).rfind(L"app-", 0) == 0) dir = Util::Parent(dir);
        return img.rfind(dir + L"\\", 0) == 0;
    }

    static std::wstring UserDataDir(const std::wstring& cmd) {
        auto i = cmd.find(L"--user-data-dir=");
        if (i == std::wstring::npos) return L"";
        // Either --user-data-dir="C:\x y" or, as some launches pass it on, "--user-data-dir=C:\x y".
        bool wholeQuoted = i > 0 && cmd[i - 1] == L'"';
        i += 16;
        std::wstring v;
        if (wholeQuoted) { auto e = cmd.find(L'"', i); v = cmd.substr(i, e == std::wstring::npos ? std::wstring::npos : e - i); }
        else if (i < cmd.size() && cmd[i] == L'"') { auto e = cmd.find(L'"', i + 1); v = cmd.substr(i + 1, e == std::wstring::npos ? std::wstring::npos : e - i - 1); }
        else { auto e = cmd.find(L' ', i); v = cmd.substr(i, e == std::wstring::npos ? std::wstring::npos : e - i); }
        while (!v.empty() && v.back() == L'\\') v.pop_back();
        return v;
    }

    // Profile dir -> pid of its top-level Claude process. Child processes carry --type=,
    // and the Claude Code CLI is also claude.exe, so both are filtered out.
    std::map<std::wstring, DWORD> RunningMap() {
        std::map<std::wstring, DWORD> map;
        if (Paths::Demo) {
            // Stand-in accounts count as open when settings.json lists them under DemoRunning.
            for (auto& id : Settings::List(L"DemoRunning")) map[Util::Lower(ProfilePath(id))] = 1;
            return map;
        }
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return map;
        PROCESSENTRY32W pe{ sizeof pe };
        auto def = Util::Lower(App.defaultData);
        while (!def.empty() && def.back() == L'\\') def.pop_back();
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            if (_wcsicmp(pe.szExeFile, L"claude.exe") != 0) continue;
            auto image = Util::ImageOf(pe.th32ProcessID);
            if (image.empty() || !IsDesktopExe(image)) continue;
            auto cmd = Util::CommandLineOf(pe.th32ProcessID);
            if (cmd.empty() || cmd.find(L"--type=") != std::wstring::npos) continue;
            auto dir = UserDataDir(cmd);
            auto key = dir.empty() ? def : Util::Lower(dir);
            if (!map.count(key)) map[key] = pe.th32ProcessID;
        }
        CloseHandle(snap);
        return map;
    }

    static std::map<std::wstring, std::wstring> Labels() {
        std::map<std::wstring, std::wstring> m;
        auto all = Settings::Load(); auto l = all.Find(L"Labels");
        if (l && l->type == json::Value::Object) for (auto& kv : l->obj) if (kv.second.type == json::Value::String) m[Util::Lower(kv.first)] = kv.second.s;
        return m;
    }

    static ULONGLONG LastUsed(const std::wstring& dir) {
        // A directory's own timestamp only moves when direct children change, so look at
        // files Electron and Claude rewrite while running.
        ULONGLONG best = 0;
        for (auto f : { L"Preferences", L"Local State", L"config.json", L"window-state.json", L"Network\\Cookies" })
            best = std::max(best, Util::FileTime(Util::Join(dir, f)));
        return best;
    }

    std::vector<Profile> ProfileList(bool withStatus) {
        std::map<std::wstring, DWORD> running;
        if (withStatus) running = RunningMap();
        auto labels = Labels();
        std::vector<std::wstring> ids{ DefaultId };
        std::vector<std::wstring> found;
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((Paths::Root + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do { if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') found.push_back(fd.cFileName); } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        std::sort(found.begin(), found.end(), [](const std::wstring& a, const std::wstring& b) {
            return CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS, a.c_str(), -1, b.c_str(), -1, nullptr, nullptr, 0) == CSTR_LESS_THAN;
        });
        ids.insert(ids.end(), found.begin(), found.end());

        std::vector<Profile> out;
        for (auto& id : ids) {
            Profile p;
            p.id = id; p.isDefault = id == DefaultId; p.path = ProfilePath(id);
            auto l = labels.find(Util::Lower(id));
            p.name = l != labels.end() && !l->second.empty() ? l->second : id;
            auto key = Util::Lower(p.path); while (!key.empty() && key.back() == L'\\') key.pop_back();
            auto r = running.find(key); if (r != running.end()) p.pid = r->second;
            p.lastUsed = LastUsed(p.path);
            p.usage = ReadUsage(p.path);
            out.push_back(p);
        }
        return out;
    }

    bool Resolve(const std::wstring& name, Profile& out) {
        auto all = ProfileList(false);
        for (auto& p : all) if (_wcsicmp(p.id.c_str(), name.c_str()) == 0) { out = p; return true; }
        for (auto& p : all) if (_wcsicmp(p.name.c_str(), name.c_str()) == 0 || _wcsicmp(DisplayName(p).c_str(), name.c_str()) == 0) { out = p; return true; }
        return false;
    }

    std::wstring DisplayName(const Profile& p) { return p.isDefault && p.name == p.id ? Tr(L"Main", L"Основной") : p.name; }

    std::wstring ValidateName(const std::wstring& name, const std::wstring& exceptId) {
        if (name.empty() || name.find_first_not_of(L' ') == std::wstring::npos) return Tr(L"The name can't be empty", L"Имя не может быть пустым");
        if (name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return Tr(L"A name can't contain \\ / : * ? \" < > |", L"В имени не может быть \\ / : * ? \" < > |");
        for (wchar_t c : name) if (c < 0x20) return Tr(L"The name contains control characters", L"В имени есть служебные символы");
        if (name[0] == L'.' || name.back() == L'.' || name.back() == L' ') return Tr(L"A name can't start with a dot or end with a dot or a space", L"Имя не может начинаться с точки или заканчиваться точкой или пробелом");
        static const wchar_t* reserved[] = { L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"LPT1", L"LPT2", L"LPT3" };
        auto base = name.substr(0, name.find(L'.'));
        for (auto r : reserved) if (_wcsicmp(base.c_str(), r) == 0) return Util::Format(Tr(L"%s is a name Windows reserves", L"%s — зарезервированное имя Windows"), Util::Q(name).c_str());
        if (name.size() > 40) return Tr(L"The name is too long, 40 characters at most", L"Имя слишком длинное, максимум 40 символов");
        if (_wcsicmp(name.c_str(), L"Add account") == 0) return Tr(L"The switcher's own shortcut already uses this name", L"Это имя занято ярлыком самого переключателя");
        for (auto& p : ProfileList(false)) {
            if (p.id == exceptId) continue;
            if (_wcsicmp(p.id.c_str(), name.c_str()) == 0 || _wcsicmp(p.name.c_str(), name.c_str()) == 0) return Util::Format(Tr(L"There is already an account called %s", L"Аккаунт %s уже есть"), Util::Q(name).c_str());
        }
        if (exceptId.empty() && Util::Exists(Paths::Root + L"\\" + name)) return Util::Format(Tr(L"The profiles folder already has %s", L"Имя %s уже занято в папке профилей"), Util::Q(name).c_str());
        return L"";
    }

    Profile AddAccount() {
        std::wstring name;
        for (int n = 2;; n++) { name = Util::Format(L"Account %d", n); if (ValidateName(name).empty()) break; }
        CreateDirectoryW(Paths::Root.c_str(), nullptr);
        CreateDirectoryW((Paths::Root + L"\\" + name).c_str(), nullptr);
        Profile p; Resolve(name, p);
        // Straight to the sign-in screen: there is nothing else to do with an empty profile.
        Start(p.id);
        return p;
    }

    static bool ShowWindowOf(DWORD pid) {
        auto wins = Util::AppWindowsOf(pid);
        if (wins.empty()) return false;
        HWND h = wins[0];
        if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
        SetForegroundWindow(h);
        return true;
    }

    bool Start(const std::wstring& id) {
        if (Paths::Demo) return false;
        auto path = ProfilePath(id);
        auto key = Util::Lower(path); while (!key.empty() && key.back() == L'\\') key.pop_back();
        auto running = RunningMap();
        auto r = running.find(key);
        bool open = r != running.end();
        // A window hidden to the tray has no visible window. Launching again is still right:
        // Electron's single instance lock hands it to the running copy.
        if (open && ShowWindowOf(r->second)) return true;
        if (!Util::Exists(App.exe)) Init(false);   // updated under us

        SHELLEXECUTEINFOW sei{ sizeof sei };
        sei.fMask = SEE_MASK_NOASYNC;
        sei.nShow = SW_SHOWNORMAL;
        std::wstring args, file, dir = Util::Parent(App.exe);
        if (id == DefaultId) {
            if (App.kind == L"Msix") { file = L"explorer.exe"; args = L"shell:AppsFolder\\" + App.aumid; }   // keeps package identity
            else file = App.exe;
        } else {
            CreateDirectoryW(path.c_str(), nullptr);
            file = App.exe; args = L"--user-data-dir=\"" + path + L"\"";
        }
        sei.lpFile = file.c_str(); sei.lpParameters = args.c_str(); sei.lpDirectory = dir.c_str();
        if (!ShellExecuteExW(&sei)) throw Util::Format(Tr(L"Couldn't start Claude (code %lu)", L"Не удалось запустить Claude (код %lu)"), GetLastError());
        return open;
    }

    // Claude's own Quit only hides it to the tray, so the window's whole process tree is
    // ended: the renderers and any Claude Code sessions it started go with it. Returns once
    // all of them are gone, so a restart isn't handed by Electron's single instance lock
    // straight to the dying copy.
    bool Close(const std::wstring& id) {
        if (Paths::Demo) return false;
        auto key = Util::Lower(ProfilePath(id)); while (!key.empty() && key.back() == L'\\') key.pop_back();
        auto running = RunningMap();
        auto r = running.find(key);
        if (r != running.end()) {
            std::vector<DWORD> tree{ r->second };
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE) {
                std::vector<PROCESSENTRY32W> all;
                PROCESSENTRY32W pe{ sizeof pe };
                for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) all.push_back(pe);
                CloseHandle(snap);
                // A parent id can outlive its process and be reused, so a child counts only if
                // it was created after the process it claims as its parent.
                auto created = [](DWORD pid) -> ULONGLONG {
                    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (!h) return 0;
                    FILETIME c, e, k, u; ULONGLONG t = 0;
                    if (GetProcessTimes(h, &c, &e, &k, &u)) t = ((ULONGLONG)c.dwHighDateTime << 32) | c.dwLowDateTime;
                    CloseHandle(h);
                    return t;
                };
                for (size_t k = 0; k < tree.size(); k++) {
                    auto parentAt = created(tree[k]);
                    for (auto& p : all) {
                        if (p.th32ParentProcessID != tree[k] || p.th32ProcessID == tree[k] || std::find(tree.begin(), tree.end(), p.th32ProcessID) != tree.end()) continue;
                        auto at = created(p.th32ProcessID);
                        if (parentAt && at && at >= parentAt) tree.push_back(p.th32ProcessID);
                    }
                }
            }
            // Ending a few hundred processes at once (MCP servers and their consoles) stalls
            // Explorer and the screen for a second or more. So the tree is frozen first, which
            // also keeps the dying copy from starting children again, then ended at idle
            // priority a batch at a time: measured on 300 processes, 40 ms of stall, not 800.
            typedef NTSTATUS(NTAPI* Fn)(HANDLE);
            static Fn suspend = (Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSuspendProcess");
            static Fn resume = (Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtResumeProcess");
            std::vector<HANDLE> hs;
            for (DWORD pid : tree) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME | PROCESS_SET_INFORMATION | SYNCHRONIZE, FALSE, pid);
                if (!h) h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
                if (!h) continue;
                if (suspend) suspend(h);
                SetPriorityClass(h, IDLE_PRIORITY_CLASS);
                hs.push_back(h);
            }
            size_t ended = 0; DWORD err = ERROR_ACCESS_DENIED;
            auto deadline = GetTickCount64() + 10000;
            for (size_t k = 0; k < hs.size(); k += 16) {
                std::vector<HANDLE> ending;
                for (size_t j = k; j < std::min<size_t>(hs.size(), k + 16); j++) {
                    if (TerminateProcess(hs[j], 0)) ending.push_back(hs[j]);
                    else { err = GetLastError(); if (resume) resume(hs[j]); }
                }
                auto now = GetTickCount64();
                if (!ending.empty()) WaitForMultipleObjects((DWORD)ending.size(), ending.data(), TRUE, now < deadline ? (DWORD)(deadline - now) : 0);
                ended += ending.size();
            }
            for (HANDLE h : hs) CloseHandle(h);
            if (!ended) throw Util::Format(Tr(L"Couldn't close Claude (code %lu)", L"Не удалось закрыть Claude (код %lu)"), err);
            return true;
        }
        return false;
    }

    void Restart(const std::wstring& id) {
        if (Paths::Demo) return;
        Close(id);
        Start(id);
    }

    static void SetLabel(const std::wstring& id, const std::wstring& label) {
        auto all = Settings::Load();
        auto& l = all[L"Labels"];
        if (l.type != json::Value::Object) l = json::Value::MakeObject();
        l.Remove(id);
        if (!label.empty() && label != id) l[id] = label;
        Settings::Save(all);
    }

    Profile Rename(const Profile& p, const std::wstring& newName) {
        auto err = ValidateName(newName, p.id);
        if (!err.empty()) throw err;
        auto removed = RemoveShortcuts(p.name);
        SetLabel(p.id, newName);
        Profile renamed; Resolve(p.id, renamed);
        renamed.pid = p.pid; renamed.usage = p.usage;
        // Shortcuts are named after the label, so put back any that were just taken away.
        for (auto& lnk : removed) CreateProfileShortcut(renamed, Util::Parent(lnk));
        return renamed;
    }

    static void RemoveTree(const std::wstring& dir) {
        std::wstring from = dir; from.push_back(L'\0');
        SHFILEOPSTRUCTW op{}; op.wFunc = FO_DELETE; op.pFrom = from.c_str();
        op.fFlags = FOF_NO_UI;
        SHFileOperationW(&op);
    }

    static void RemoveIcons(const std::wstring& id) {
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((Paths::IconDir + L"\\*.ico").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        auto lid = Util::Lower(id);
        do {
            std::wstring base = Util::Lower(fd.cFileName); base = base.substr(0, base.size() - 4);
            if (base == lid || (base.rfind(lid + L"-", 0) == 0 && base.size() <= lid.size() + 3)) DeleteFileW((Paths::IconDir + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    void Delete(const Profile& p) {
        if (p.isDefault) throw std::wstring(Tr(L"The main account can't be deleted", L"Основной аккаунт удалить нельзя"));
        auto path = Paths::Root + L"\\" + p.id;
        // Never recurse into anything that is not a direct child of our own folder.
        if (p.id.empty() || p.id[0] == L'.' || p.id.find_first_of(L"\\/") != std::wstring::npos || !Paths::Same(Util::Parent(path), Paths::Root))
            throw Util::Format(Tr(L"Refusing to delete %s", L"Отказ удалять %s"), Util::Q(path).c_str());
        auto key = Util::Lower(path);
        if (RunningMap().count(key)) throw Util::Format(Tr(L"Close the %s window first", L"Сначала закройте окно %s"), Util::Q(p.name).c_str());
        if (Util::IsDir(path)) RemoveTree(path);
        RemoveShortcuts(p.name);
        RemoveIcons(p.id);
        SetLabel(p.id, L"");
    }

    // ============================================================ colours

    static const COLORREF Palette[] = { RGB(66, 133, 180), RGB(76, 150, 96), RGB(150, 96, 180), RGB(190, 80, 110),
                                        RGB(60, 150, 150), RGB(90, 110, 200), RGB(200, 150, 50), RGB(80, 86, 100) };

    COLORREF ProfileColor(const std::wstring& id) {
        // Assigned once and remembered, so two profiles rarely share a colour and a
        // colour never changes later.
        auto all = Settings::Load();
        auto& colors = all[L"Colors"];
        if (colors.type != json::Value::Object) colors = json::Value::MakeObject();
        for (auto& kv : colors.obj) if (_wcsicmp(kv.first.c_str(), id.c_str()) == 0) return Palette[(int)kv.second.n % 8];
        int use[8] = {};
        std::set<std::wstring> alive;
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((Paths::Root + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) { do alive.insert(Util::Lower(fd.cFileName)); while (FindNextFileW(h, &fd)); FindClose(h); }
        for (auto& kv : colors.obj) if (alive.count(Util::Lower(kv.first))) use[(int)kv.second.n % 8]++;
        int pick = 0; for (int i = 1; i < 8; i++) if (use[i] < use[pick]) pick = i;
        colors[id] = pick;
        Settings::Save(all);
        return Palette[pick];
    }

    // Initials: "Work" gives W, "Account 3" gives A3, "Рабочий профиль" gives РП.
    std::wstring BadgeLetter(const std::wstring& label) {
        std::vector<wchar_t> firsts; bool start = true;
        for (wchar_t c : label) {
            if (c == L' ' || c == L'-' || c == L'_' || c == L'.') { start = true; continue; }
            if (start && (IsCharAlphaNumericW(c))) { firsts.push_back(c); start = false; }
            else if (start) continue;
        }
        std::wstring s;
        if (firsts.empty()) s = label.substr(0, 1);
        else if (firsts.size() == 1) s = std::wstring(1, firsts[0]);
        else { s += firsts.front(); s += firsts.back(); }
        CharUpperBuffW(&s[0], (DWORD)s.size());
        return s;
    }

    // ============================================================ chats

    /*
        The desktop app keeps one small JSON file per Claude Code chat, at
            <profile>\claude-code-sessions\<account id>\<organisation id>\local_<id>.json
        The transcript itself is kept by the Claude Code CLI, outside the profile. Copying a
        chat to another account places that file into the other profile's store, under the
        account and organisation that profile is signed in to.
    */
    static std::vector<std::wstring> Dirs(const std::wstring& root) {
        std::vector<std::wstring> out;
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((root + L"\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return out;
        do if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != L'.') out.push_back(fd.cFileName); while (FindNextFileW(h, &fd));
        FindClose(h);
        return out;
    }

    static std::wstring First(const json::Value& v, std::initializer_list<const wchar_t*> keys) {
        for (auto k : keys) { auto s = v.Str(k); if (s.find_first_not_of(L' ') != std::wstring::npos) return s; }
        return L"";
    }

    std::vector<CodeSession> CodeSessions(const Profile& p) {
        std::map<std::wstring, CodeSession> found;
        auto root = p.path + L"\\claude-code-sessions";
        for (auto& account : Dirs(root))
        for (auto& org : Dirs(root + L"\\" + account)) {
            auto dir = root + L"\\" + account + L"\\" + org;
            WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\local_*.json").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                CodeSession s;
                s.fileName = fd.cFileName; s.file = dir + L"\\" + fd.cFileName; s.account = account; s.org = org;
                s.updated = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
                json::Value v; json::Parser::Parse(Util::ReadText(s.file, true), v);
                s.title = First(v, { L"title", L"name", L"summary", L"customTitle", L"displayName" });
                auto folder = First(v, { L"cwd", L"workingDirectory", L"projectPath", L"folder", L"path" });
                s.folder = folder.empty() ? L"" : Util::Lower(folder).find(L"\\scratch-workspaces\\") != std::wstring::npos ? Tr(L"no folder", L"без папки") : Util::Leaf(folder);
                if (s.title.empty()) s.title = Tr(L"Chat ", L"Чат ") + s.fileName.substr(6, std::min<size_t>(8, s.fileName.size() - 11));
                // The same chat can sit under two accounts after an earlier copy. List it once.
                auto key = Util::Lower(s.fileName);
                if (!found.count(key) || found[key].updated < s.updated) found[key] = s;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        std::vector<CodeSession> out;
        for (auto& kv : found) out.push_back(kv.second);
        std::sort(out.begin(), out.end(), [](const CodeSession& a, const CodeSession& b) { return a.updated > b.updated; });
        return out;
    }

    std::wstring CodeSessionStore(const Profile& p) {
        // A profile signed in to more than one account keeps a folder for each; the one
        // written to most recently belongs to whoever is signed in now.
        auto root = p.path + L"\\claude-code-sessions";
        std::wstring best; ULONGLONG bestTime = 0;
        for (auto& a : Dirs(root)) for (auto& o : Dirs(root + L"\\" + a)) {
            auto d = root + L"\\" + a + L"\\" + o; auto t = Util::FileTime(d);
            if (best.empty() || t > bestTime) { best = d; bestTime = t; }
        }
        return best;
    }

    static void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to) {
        if (from.empty()) return;
        for (size_t i = 0; (i = s.find(from, i)) != std::wstring::npos; i += to.size()) s.replace(i, from.size(), to);
    }

    bool CopyCodeSession(const CodeSession& s, const std::wstring& store) {
        auto dest = store + L"\\" + s.fileName;
        if (Util::Exists(dest)) return false;
        auto text = Util::ReadText(s.file, true);
        // Only whole JSON string values are swapped, so paths that merely contain the same
        // ids (scratch folders are named after them) are left alone.
        ReplaceAll(text, L"\"" + s.account + L"\"", L"\"" + Util::Leaf(Util::Parent(store)) + L"\"");
        ReplaceAll(text, L"\"" + s.org + L"\"", L"\"" + Util::Leaf(store) + L"\"");
        if (!Util::WriteText(dest, text, false)) throw Tr(L"Couldn't write the chat to ", L"Не удалось записать чат в ") + store;
        return true;
    }

    // ============================================================ usage

    long long NowMs() {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        return (long long)((((ULONGLONG)ft.dwHighDateTime << 32 | ft.dwLowDateTime) - 116444736000000000ull) / 10000);
    }

    std::wstring Age(long long ms) {
        if (ms <= 0) return L"";
        long long mins = (NowMs() - ms) / 60000;
        if (mins < 2) return Tr(L"just now", L"только что");
        if (mins < 60) return Util::Format(Tr(L"%lld min ago", L"%lld мин назад"), mins);
        ULONGLONG ft = (ULONGLONG)ms * 10000 + 116444736000000000ull;
        FILETIME f{ (DWORD)ft, (DWORD)(ft >> 32) }; SYSTEMTIME u, t, now;
        FileTimeToSystemTime(&f, &u); SystemTimeToTzSpecificLocalTime(nullptr, &u, &t); GetLocalTime(&now);
        if (mins < 20 * 60 && t.wDay == now.wDay) return Tr(L"at ", L"в ") + Util::Clock(t);
        static const wchar_t* m[] = { L"янв", L"фев", L"мар", L"апр", L"мая", L"июн", L"июл", L"авг", L"сен", L"окт", L"ноя", L"дек" };
        static const wchar_t* en[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };
        if (!L10n::ru) return Util::Format(L"%s %d, ", en[t.wMonth - 1], t.wDay) + Util::Clock(t);
        return Util::Format(L"%d %s, %02d:%02d", t.wDay, m[t.wMonth - 1], t.wHour, t.wMinute);
    }

    // "2026-10-13T10:00:00.320078+00:00" -> unix ms; 0 if it does not parse.
    static long long ParseIso(const std::wstring& s) {
        int y, mo, d, h, mi, se; wchar_t sign = 0; int oh = 0, om = 0;
        if (swscanf_s(s.c_str(), L"%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
        auto tz = s.find_first_of(L"+-Z", 19);
        if (tz != std::wstring::npos && s[tz] != L'Z') { sign = s[tz]; swscanf_s(s.c_str() + tz + 1, L"%d:%d", &oh, &om); }
        SYSTEMTIME st{ (WORD)y, (WORD)mo, 0, (WORD)d, (WORD)h, (WORD)mi, (WORD)se, 0 };
        FILETIME ft; if (!SystemTimeToFileTime(&st, &ft)) return 0;
        long long ms = (long long)((((ULONGLONG)ft.dwHighDateTime << 32 | ft.dwLowDateTime) - 116444736000000000ull) / 10000);
        long long off = (oh * 60ll + om) * 60000;
        return sign == L'+' ? ms - off : sign == L'-' ? ms + off : ms;
    }

    /*
        Where plan usage can be found on this computer, freshest wins, per figure:

          <profile>\plan-usage-history.json   the desktop app's own record, per profile:
              { "samples": [ { "t": <unix ms>, "u": { "fh": 75, "sd": 55 } } ] }
              fh = five-hour window, sd = seven-day window, percent used. Written only now
              and then, and a sample may carry one figure and not the other.
          %USERPROFILE%\.claude.json            Claude Code's last answer from the server:
              "cachedUsageUtilization": { "fetchedAtMs", "accountUuid",
                "utilization": { "five_hour": { "utilization", "resets_at" }, "seven_day": {...} } }
              It is for one account only, matched through the profile's lastKnownAccountUuid.

        A figure stops applying once its window resets: at resets_at when known, otherwise
        once a full window has passed since it was taken. After that it is shown as unknown,
        since the account may have been used on the web or in the CLI in the meantime.
    */
    Usage ReadUsage(const std::wstring& dir) {
        Usage u;
        long long now = NowMs(), fiveReset = 0, weekReset = 0;
        double five = -1, week = -1;

        json::Value v;
        if (json::Parser::Parse(Util::ReadText(dir + L"\\plan-usage-history.json", true), v)) {
            if (auto samples = v.Find(L"samples")) for (auto& s : samples->arr) {
                auto vals = s.Find(L"u"); long long t = (long long)s.Num(L"t", 0);
                if (!vals || !t) continue;
                u.known = true;
                if (vals->Find(L"fh") && t > u.fiveAt) { u.fiveAt = t; five = vals->Num(L"fh"); }
                if (vals->Find(L"sd") && t > u.weekAt) { u.weekAt = t; week = vals->Num(L"sd"); }
            }
        }

        json::Value cfg;
        json::Parser::Parse(Util::ReadText(dir + L"\\config.json", true), cfg);
        auto account = cfg.Str(L"lastKnownAccountUuid");
        static std::wstring cliPath = [] { wchar_t b[MAX_PATH]; GetEnvironmentVariableW(L"USERPROFILE", b, MAX_PATH); return std::wstring(b) + L"\\.claude.json"; }();
        json::Value cli;
        if (!Paths::Demo && !account.empty() && json::Parser::Parse(Util::ReadText(cliPath, true), cli)) {
            auto c = cli.Find(L"cachedUsageUtilization");
            auto util = c ? c->Find(L"utilization") : nullptr;
            long long at = c ? (long long)c->Num(L"fetchedAtMs", 0) : 0;
            if (util && at && _wcsicmp(c->Str(L"accountUuid").c_str(), account.c_str()) == 0) {
                u.known = true;
                if (auto f = util->Find(L"five_hour")) if (at > u.fiveAt && f->Find(L"utilization")) { u.fiveAt = at; five = f->Num(L"utilization"); fiveReset = ParseIso(f->Str(L"resets_at")); }
                if (auto w = util->Find(L"seven_day")) if (at > u.weekAt && w->Find(L"utilization")) { u.weekAt = at; week = w->Num(L"utilization"); weekReset = ParseIso(w->Str(L"resets_at")); }
            }
        }

        // Shown only when current: taken in the last 15 minutes and before its window reset.
        // Anything older is not shown at all rather than shown as if it were now.
        const long long fresh = 15ll * 60 * 1000;
        bool fiveLive = u.fiveAt && now - u.fiveAt <= fresh && (!fiveReset || now < fiveReset);
        bool weekLive = u.weekAt && now - u.weekAt <= fresh && (!weekReset || now < weekReset);
        u.fiveHour = fiveLive ? five : -1;
        u.week = weekLive ? week : -1;
        u.known = fiveLive || weekLive;
        return u;
    }

    void Log(const std::wstring& text) {
        CreateDirectoryW(Paths::Root.c_str(), nullptr);
        SYSTEMTIME t; GetLocalTime(&t);
        auto line = Util::Format(L"[%04d-%02d-%02dT%02d:%02d:%02d] ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond) + text + L"\r\n\r\n";
        auto old = Util::ReadText(Paths::Log);
        Util::WriteText(Paths::Log, old + line, false);
    }
}
