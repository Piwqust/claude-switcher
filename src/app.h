// The running switcher: shared account list, tray icon, hotkeys and every action the
// windows offer.
#pragma once
#include "gfx.h"

struct Item {
    Profile p;
    std::wstring name, letters;     // display name ("Main" for Default) and avatar initials
    COLORREF color = 0;
    int index = 0;                  // 1-based; the digit that opens it, for the first nine
    bool launching = false, closing = false, checked = false, autoOpen = false;
    ULONGLONG launchAt = 0;
    DWORD endingPid = 0;            // while restarting: the old window's pid, which still counts as starting
    bool running() const { return p.pid != 0; }
    std::wstring Status() const;
};

enum class StartMode { Window, Tray, Startup };

inline const wchar_t* AppTitle() { return Tr(L"Claude Accounts", L"Аккаунты Claude"); }

namespace App {
    extern std::vector<Item> items;
    extern HWND tray;
    extern HICON smallIcon;

    void Start(StartMode mode);
    void Refresh();
    Item* Find(const std::wstring& id);

    void ShowPicker(bool atSignIn = false);
    void ClosePicker();
    void ShowSettings();
    void Exit();
    void ThemeChanged();

    void Launch(const std::vector<std::wstring>& ids, bool fromPicker);
    void Restart(const std::vector<std::wstring>& ids);
    void Close(const std::vector<std::wstring>& ids);
    void AddAccount();
    void BeginRename(const std::wstring& id);
    void CommitRename(const std::wstring& id, const std::wstring& name);
    void Delete(const std::wstring& id, HWND owner);
    void ToggleAutoOpen(const std::wstring& id);
    void ShowMenu(const std::wstring& id, POINT screen, HWND owner);
    void Notify(const std::wstring& title, const std::wstring& text, bool error = false);
    void CopyChats(HWND owner, const std::vector<CodeSession>& picked, const std::wstring& targetId);

    // settings
    bool Autostart(); void SetAutostart(bool on);
    bool AskAtStartup(); void SetAskAtStartup(bool on);
    bool ShowAtStartup(); void SetShowAtStartup(bool on);
    int Countdown(); void SetCountdown(int s);
    bool HideAfterLaunch(); void SetHideAfterLaunch(bool on);
    bool KeepInTray(); void SetKeepInTray(bool on);
    bool SplitTaskbar(); void SetSplitTaskbar(bool on);
    std::wstring Hotkey(); bool SetHotkey(const std::wstring& combo); bool HotkeyFailed();
    void SuspendHotkey(); void ResumeHotkey();
    bool AccountHotkeys(); void SetAccountHotkeys(bool on); std::vector<int> AccountHotkeysTaken();
    void SaveAutoOpen();
    void SetLanguage(const std::wstring& choice);   // L"auto", L"en" or L"ru"; applies at once
}

// Windows
struct PickerWindow;
namespace Picker {
    void Show(bool atSignIn, const std::vector<std::wstring>& autoOpen);
    void Close();
    bool Visible();
    bool Active();
    void NewAccountHint(const std::wstring& id);
    void Rename(const std::wstring& id);
    void Restyle();
    void Invalidate();
    HWND Handle();
}
namespace Flyout { void Toggle(); void Hide(); bool Visible(); void Restyle(); void Invalidate(); }
namespace SettingsWin { void Show(); bool Visible(); void Restyle(); void Invalidate(); }
namespace Dialogs {
    bool Ask(HWND owner, const std::wstring& title, const std::wstring& text, const std::wstring& yes, bool danger = false, const std::wstring& no = Tr(L"Cancel", L"Отмена"));
    void Info(HWND owner, const std::wstring& title, const std::wstring& text);
    void Transfer(HWND owner, const std::wstring& sourceId);
    void RestyleAll();
}

namespace Hotkeys {
    bool Parse(const std::wstring& combo, UINT& mods, UINT& vk);
    std::wstring Pretty(const std::wstring& combo);
    std::wstring FromKeys(UINT mods, UINT vk);
}
