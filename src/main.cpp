/*
    ClaudeProfileSwitcher.exe                 open the picker (installs itself on first run)
    ClaudeProfileSwitcher.exe --startup       what Windows runs at sign-in
    ClaudeProfileSwitcher.exe --tray          start in the notification area only
    ClaudeProfileSwitcher.exe --launch Work   open an account, or bring it forward, then exit
    ClaudeProfileSwitcher.exe --restart Work  close an account's Claude and open it again, then exit
    ClaudeProfileSwitcher.exe --add-account   add an account (in the running switcher if there is one)
    ClaudeProfileSwitcher.exe --install       install or refresh the install, then open the picker
    ClaudeProfileSwitcher.exe --claude-path <exe>   point at Claude.exe if detection fails (remembered)

    CPS_DEMO=<folder>   environment variable: a separate switcher over stand-in accounts (see core.cpp)
*/
#include "app.h"
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>

static void Fail(const std::wstring& what) {
    Core::Log(what);
    MessageBoxW(nullptr, (what + Tr(L"\n\nDetails were written to ", L"\n\nПодробности записаны в ") + Paths::Log).c_str(), AppTitle(), MB_OK | MB_ICONERROR);
}

// Finds Claude, asking where Claude.exe is if it cannot.
static bool EnsureClaude(bool allowCache) {
    for (;;) {
        try { Core::Init(allowCache); return true; }
        catch (Core::NotFound&) {
            if (MessageBoxW(nullptr, Tr(L"Couldn't find the Claude app on this PC.\n\nPoint to Claude.exe yourself? The choice is remembered.",
                                        L"Не удалось найти приложение Claude на этом компьютере.\n\nУказать путь к Claude.exe вручную? Выбор запомнится."),
                            AppTitle(), MB_YESNO | MB_ICONWARNING) != IDYES) return false;
            wchar_t file[MAX_PATH * 2] = {};
            OPENFILENAMEW of{ sizeof of };
            of.lpstrFilter = Tr(L"Claude.exe\0Claude.exe\0Programs\0*.exe\0", L"Claude.exe\0Claude.exe\0Программы\0*.exe\0"); of.lpstrFile = file; of.nMaxFile = ARRAYSIZE(file);
            of.lpstrTitle = Tr(L"Where is Claude.exe?", L"Где находится Claude.exe?"); of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (!GetOpenFileNameW(&of)) return false;
            Settings::Set(L"ClaudePath", std::wstring(file));
        }
    }
}

static int Run() {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    auto flag = [&](const wchar_t* n) { for (int i = 1; i < argc; i++) if (_wcsicmp(argv[i], n) == 0) return true; return false; };
    auto arg = [&](const wchar_t* n) -> std::wstring { for (int i = 1; i + 1 < argc; i++) if (_wcsicmp(argv[i], n) == 0) return argv[i + 1]; return L""; };

    auto appIcon = arg(L"--make-app-icon");
    if (!appIcon.empty()) { Gfx::Init(); Core::WriteAppIcon(appIcon); return 0; }
    auto samples = arg(L"--icon-samples");
    if (!samples.empty()) { if (!EnsureClaude(true)) return 1; Gfx::Init(); Core::WriteIconSamples(samples); return 0; }

    auto claudePath = arg(L"--claude-path");
    if (!claudePath.empty()) {
        if (!Util::Exists(claudePath)) throw Util::Format(Tr(L"--claude-path points to %s, which doesn't exist", L"--claude-path указывает на %s, такого файла нет"), Util::Q(claudePath).c_str());
        Settings::Set(L"ClaudePath", claudePath);
    }

    auto launch = arg(L"--launch");
    if (!launch.empty()) {
        // Shortcuts and the jump list land here: no window, no graphics, straight to Claude.
        if (!EnsureClaude(true)) return 1;
        Profile p;
        if (!Core::Resolve(launch, p)) throw Util::Format(Tr(L"There is no account called %s", L"Аккаунта %s нет"), Util::Q(launch).c_str());
        Core::Start(p.id);
        return 0;
    }

    auto restart = arg(L"--restart");
    if (!restart.empty()) {
        if (!EnsureClaude(true)) return 1;
        Profile p;
        if (!Core::Resolve(restart, p)) throw Util::Format(Tr(L"There is no account called %s", L"Аккаунта %s нет"), Util::Q(restart).c_str());
        Core::Restart(p.id);
        return 0;
    }

    if (flag(L"--add-account")) {
        // The running switcher does it, so the new card can be named straight away.
        if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, Core::AddEvent)) { AllowSetForegroundWindow(ASFW_ANY); SetEvent(e); CloseHandle(e); return 0; }
        if (!EnsureClaude(false)) return 1;
        Core::AddAccount();
        return 0;
    }

    if (!EnsureClaude(false)) return 1;
    Gfx::Init();

    // Run from anywhere other than its install folder (a fresh download, a build output),
    // the exe installs itself and hands over to the installed copy.
    bool installed = Paths::Demo || Paths::Same(Paths::Self, Paths::InstalledExe);
    if (flag(L"--install") || (!installed && argc <= 1)) {
        Core::Install();
        ShellExecuteW(nullptr, L"open", Paths::InstalledExe.c_str(), nullptr, Paths::Home.c_str(), SW_SHOWNORMAL);
        return 0;
    }

    auto mode = flag(L"--startup") ? StartMode::Startup : flag(L"--tray") ? StartMode::Tray : StartMode::Window;

    // One switcher at a time. A second copy asks the first to show itself and leaves.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, Core::MutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mode == StartMode::Window) {
            if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, Core::ShowEvent)) { AllowSetForegroundWindow(ASFW_ANY); SetEvent(e); CloseHandle(e); }
        }
        CloseHandle(mutex);
        return 0;
    }

    App::Start(mode);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    ReleaseMutex(mutex); CloseHandle(mutex);
    return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Paths::Init();
    L10n::Init();
    int code = 1;
    try { code = Run(); }
    catch (const std::wstring& e) { Fail(e); }
    catch (const wchar_t* e) { Fail(e); }
    catch (const std::exception& e) { Fail(Util::Format(L"%S", e.what())); }
    CoUninitialize();
    return code;
}
