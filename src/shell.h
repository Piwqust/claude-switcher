#pragma once
#include "core.h"

namespace WinShell {
    void UpdateJumpList(const std::vector<Profile>& profiles);
    void DarkMenus(bool dark);                              // native popup menus follow the theme
    void AllowDark(HWND h, bool dark);

    // Separate taskbar buttons per account (see shell.cpp for how and why).
    void SplitUpdate(const std::vector<Profile>& profiles, bool on);
    void SplitShutdown();
}
