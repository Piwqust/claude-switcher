# Contributing

Thanks for stopping by. Bug reports, ideas and pull requests are all welcome.

## Reporting a bug

Open an [issue](https://github.com/Piwqust/claude-profile-switcher/issues/new/choose) from the template. The most useful details are:

- the switcher's version (Settings → About) and your Windows version;
- which Claude build you have: Microsoft Store or the installer;
- the steps that lead to the problem;
- the contents of `%LOCALAPPDATA%\ClaudeProfiles\switcher-error.log`, if it exists.

Please do not attach profile folders or `.claude.json`: they contain your accounts' data.

## Building

You need Visual Studio 2022 or the Build Tools with "Desktop development with C++". ARM64 also needs "MSVC ARM64 build tools".

```bat
build.cmd           :: build\x64\ClaudeProfileSwitcher.exe
build.cmd arm64     :: build\arm64\ClaudeProfileSwitcher.exe
```

The build is static (`/MT`), so the exe needs nothing on the target machine. Keep an eye on its size: it must stay under 1 MB, and CI checks that.

## How the code is laid out

| File | What is in it |
| --- | --- |
| `src/main.cpp` | Command line, single instance, message loop |
| `src/core.*` | Paths, settings, the interface language, finding Claude, profiles, launching, renaming, usage |
| `src/shell.*`, `src/icons.inc` | Shortcuts, start with Windows, install, jump list, taskbar buttons, icon drawing |
| `src/gfx.*` | Direct2D and DirectWrite, palettes, the immediate-mode UI layer, shared controls |
| `src/app.*` | App state, tray, global shortcuts, background refresh |
| `src/picker.cpp`, `flyout.cpp`, `settings.cpp`, `dialogs.cpp` | The picker, the tray flyout, Settings, dialogs |
| `src/json.h` | A small JSON reader and writer |

Every window draws itself in `Build()`. Interactive parts register a region with `Hit()`, and hover, press and focus are animated per region id. Slow work (the process list, profile files) runs on a worker thread and hands its result back to the window with a message.

## Interface text

Every string sits in the code in both languages: `Tr(L"English", L"Русский")`. Add both whenever you add one. Plurals go through `Util::Count`, quotes through `Util::Q` (“…” in English, «…» in Russian), dates through `Util::When` and `Core::Age`.

Keep text short and plain: buttons name the action ("Open now"), labels name the data, and errors say what happened and what to do next. Want another language? Open an issue first so we can agree on how to structure it.

## Before opening a pull request

1. Build x64, and ARM64 too if you changed anything platform-specific.
2. Try your change in the demo, in both themes and both languages:
   ```powershell
   .\tools\demo.ps1 -Theme dark
   .\tools\demo.ps1 -Theme light -Language ru
   ```
   The demo runs over made-up accounts in `%TEMP%\cps-demo`. It never starts Claude and leaves your shortcuts, start with Windows, global shortcuts and the real switcher alone.
3. If you changed how anything looks, regenerate the pictures in `docs/images`: `.\tools\make-images.ps1`. It captures only the demo's own windows with PrintWindow, so nothing else on your screen can end up in them.
4. Add a line under "Unreleased" in [CHANGELOG.md](CHANGELOG.md).

## Windows traps that have bitten before

- **Path length.** The code uses `MAX_PATH`; the demo folder is short on purpose.
- **PowerShell 5.1 encoding.** Save scripts with non-ASCII text as UTF-8 **with a BOM**, or PowerShell reads them as ANSI. CI checks this.
- **Two `claude.exe`s.** The Claude Code CLI is also `claude.exe`. Open windows are detected only from Claude Desktop's own process without `--type=`.
- **The Store build.** The main account is started through `shell:AppsFolder`, not by running the exe, or it loses its package identity.
- **Profile names.** The folder is a profile's permanent id; renaming only changes its label in `settings.json`.

## Releasing (maintainers)

1. Move the "Unreleased" entries into a new `## [X.Y.Z] — YYYY-MM-DD` section in CHANGELOG.md.
2. Bump the version in `src/app.rc`, `src/app.manifest` and the About text in `src/settings.cpp`.
3. Create and push a tag:
   ```bash
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```
   The `release.yml` workflow builds x64 and ARM64, computes SHA-256 checksums and publishes the release with its notes from CHANGELOG.md.
