// Profiles, discovery, launching and everything the switcher keeps on disk.
// Same files and names as the earlier PowerShell and .NET versions, so an upgrade
// keeps every account, label, colour and shortcut.
#pragma once
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include "json.h"

struct ClaudeInstall {
    std::wstring kind;          // L"Msix" (Store) or L"Classic" (installer)
    std::wstring exe, version, aumid, defaultData;
};

// Plan usage as recorded on this computer. Claude records it only now and then, so a
// figure counts only while it is current (see ReadUsage); otherwise it is not shown.
struct Usage {
    bool known = false;                // at least one current figure
    double fiveHour = -1, week = -1;   // percent used; -1 = no figure that still applies
    long long fiveAt = 0, weekAt = 0;  // unix ms the figures were taken
    long long newest() const { return fiveAt > weekAt ? fiveAt : weekAt; }
};

struct Profile {
    std::wstring id, name, path;
    bool isDefault = false;
    DWORD pid = 0;
    ULONGLONG lastUsed = 0;            // FILETIME ticks, 0 = never
    Usage usage;
};

struct CodeSession {
    std::wstring file, fileName, account, org, title, folder;
    ULONGLONG updated = 0;
};

// Interface language: English, or Russian when Windows itself is in Russian. Settings can
// pin either one (and CPS_LANG=en|ru does, for screenshots). Strings sit in the code as
// Tr(english, russian), so both stay side by side and neither can be forgotten.
namespace L10n {
    extern bool ru;
    void Init();                                   // after Paths::Init
    std::wstring Choice();                         // L"auto", L"en" or L"ru"
    void Choose(const std::wstring& choice);
}
inline const wchar_t* Tr(const wchar_t* en, const wchar_t* ru) { return L10n::ru ? ru : en; }

namespace Paths {
    extern std::wstring Root, Home, IconDir, InstalledExe, SettingsFile, Log, Self;
    extern bool Demo;                  // CPS_DEMO is set: stand-in accounts, nothing outside that folder is touched
    void Init();
    std::wstring Launcher();
    std::wstring Desktop();
    std::wstring StartMenu();
    std::wstring Startup();
    bool Same(const std::wstring& a, const std::wstring& b);
}

namespace Settings {
    json::Value Load();
    void Save(const json::Value& v);
    void Set(const std::wstring& key, json::Value value);
    bool Flag(const std::wstring& key, bool def);
    int Int(const std::wstring& key, int def);
    std::wstring Str(const std::wstring& key, const std::wstring& def = L"");
    bool Has(const std::wstring& key);
    std::vector<std::wstring> List(const std::wstring& key);
    void SetList(const std::wstring& key, const std::vector<std::wstring>& items);
}

namespace Util {
    std::wstring ReadText(const std::wstring& path, bool shared = false);
    bool WriteText(const std::wstring& path, const std::wstring& text, bool bom);
    bool Exists(const std::wstring& path);
    bool IsDir(const std::wstring& path);
    std::wstring Parent(const std::wstring& path);
    std::wstring Leaf(const std::wstring& path);
    std::wstring Join(const std::wstring& a, const std::wstring& b);
    std::wstring Lower(std::wstring s);
    std::wstring Format(const wchar_t* fmt, ...);
    ULONGLONG FileTime(const std::wstring& path);
    std::wstring CommandLineOf(DWORD pid);
    std::wstring ImageOf(DWORD pid);
    std::vector<HWND> AppWindowsOf(DWORD pid);
    std::wstring Plural(int n, const wchar_t* one, const wchar_t* few, const wchar_t* many);   // Russian forms
    // "3 accounts" / "3 аккаунта": the number with the word in the current language.
    std::wstring Count(int n, const wchar_t* enOne, const wchar_t* enMany, const wchar_t* ruOne, const wchar_t* ruFew, const wchar_t* ruMany);
    std::wstring Q(const std::wstring& s);         // “quoted” or «в кавычках»
    std::wstring When(ULONGLONG fileTime);
    std::wstring Clock(const SYSTEMTIME& t);
    std::wstring Names(const std::vector<std::wstring>& names);
}

namespace Core {
    extern ClaudeInstall App;
    extern const wchar_t* DefaultId;
    extern const wchar_t* MutexName;
    extern const wchar_t* ShowEvent;
    extern const wchar_t* ExitEvent;
    extern const wchar_t* AddEvent;

    struct NotFound {};
    void Init(bool allowCache);                    // throws NotFound

    std::wstring ProfilePath(const std::wstring& id);
    std::map<std::wstring, DWORD> RunningMap();    // lower-case profile dir -> pid
    std::vector<Profile> ProfileList(bool withStatus);
    bool Resolve(const std::wstring& name, Profile& out);
    std::wstring DisplayName(const Profile& p);    // Default reads as "Main" / "Основной"
    std::wstring ValidateName(const std::wstring& name, const std::wstring& exceptId = L"");
    Profile AddAccount();
    bool Start(const std::wstring& id);            // true when an open window was brought forward
    void Restart(const std::wstring& id);          // ends the open window, if any, and starts it again
    Profile Rename(const Profile& p, const std::wstring& newName);
    void Delete(const Profile& p);                 // throws std::wstring on refusal

    COLORREF ProfileColor(const std::wstring& id);
    std::wstring BadgeLetter(const std::wstring& label);
    std::wstring ProfileIconPath(const Profile& p);
    std::wstring AddAccountIconPath();
    void WriteAppIcon(const std::wstring& path);   // the switcher's own icon, used at build time
    void WriteIconSamples(const std::wstring& dir);
    std::wstring CreateProfileShortcut(const Profile& p, const std::wstring& dir);
    std::vector<std::wstring> RemoveShortcuts(const std::wstring& label);

    bool AutostartEnabled();
    void SetAutostart(bool on);
    void Install();

    std::vector<CodeSession> CodeSessions(const Profile& p);
    std::wstring CodeSessionStore(const Profile& p);
    bool CopyCodeSession(const CodeSession& s, const std::wstring& store);

    Usage ReadUsage(const std::wstring& profileDir);
    long long NowMs();
    std::wstring Age(long long unixMs);            // "только что", "12 мин назад", "в 07:42", "14 окт"
    void Log(const std::wstring& text);
}
