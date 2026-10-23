# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [Semantic Versioning](https://semver.org/).
Each version's section becomes the description of its GitHub release.

## [Unreleased]

### Fixed

- Restarting or closing Claude no longer freezes Windows for a couple of seconds when Claude has many MCP servers running. The processes are now ended a few at a time instead of all at once.

## [1.0.2] — 2026-10-20

### New

- **A restart button in the tray flyout** next to every open account.
- **Close Claude for good**, tray icon included: ✕ in the tray flyout, Close Claude in the tray menu (one account or all open ones) and in a card's ⋯ menu, or `--close "Work"` on the command line.

## [1.0.1] — 2026-10-20

### New

- **Restart Claude** in one click: from the tray menu (one account or all open ones), from a card's ⋯ menu, with <kbd>Ctrl</kbd>+<kbd>R</kbd> on a card, or `--restart "Work"` on the command line. It ends the account's whole Claude process, Claude Code chats included, and opens it again.

### Fixed

- An account started with `"--user-data-dir=…"` quoted as a whole was not recognised as open.

## [1.0.0] — 2026-10-16

The first release. Claude Profile Switcher is built on [Claude Code Desktop Switcher](https://github.com/PriyanshuGeTRekT/Claude-Code-Desktop-Switcher) 2.0 by @PriyanshuGeTRekT, a PowerShell script, and rewrites it from scratch as a native C++ app. Accounts, names, colours and shortcuts from that switcher carry over: just run the new exe.

### Faster and lighter

- **One native exe** (C++, Win32, Direct2D, DirectWrite): about 550 KB with no .NET, PowerShell or other runtime. Builds for x64 and ARM64.
- **The picker opens in about 75 ms** instead of about 10 s, and the switcher uses about 16 MB of memory instead of about 126 MB (same PC, three accounts).
- **No console window and no execution policy**: run the exe and it installs itself.

### New

- **English and Russian interface.** Follows the Windows display language; Settings → Language pins either one.
- **A card-based picker** in the Windows 11 style: dark and light themes from the system setting, Mica, rounded corners, smooth hover and press states.
- **Choose at Windows sign-in.** The switcher starts with Windows and asks which account to open. Ticked accounts open on their own after a 3, 5 or 10 second countdown that any key stops.
- **Several accounts at once**: <kbd>Ctrl</kbd>+click or <kbd>Space</kbd> on the cards, then one button.
- **Global shortcuts**: <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd> for the picker (configurable) and <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>…<kbd>9</kbd> for accounts.
- **Usage on the cards**: 5-hour and 7-day usage, shown only while at most 15 minutes old.
- **A tray flyout** on left-click and a themed system menu on right-click.
- **A jump list** on the taskbar with every account.
- **Separate taskbar buttons per account** (experimental).
- **New icons**: the switcher, the main account, and coloured badges with initials.
- **Rename in place** on the card with <kbd>F2</kbd>.
- **Demo mode** (`CPS_DEMO`, `tools\demo.ps1`): the interface over made-up accounts, for screenshots and testing.

### Changed from the PowerShell switcher

- The original account is called "Main"; new ones are `Account 2`, `Account 3` and so on, and can be renamed.
- Start with Windows uses `HKCU\…\Run` instead of a shortcut in the Startup folder. The old shortcut is removed.
- Messages appear as notifications and dialogs instead of a status line at the bottom of the window.

[Unreleased]: https://github.com/Piwqust/claude-switcher/compare/v1.0.2...HEAD
[1.0.2]: https://github.com/Piwqust/claude-switcher/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/Piwqust/claude-switcher/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/Piwqust/claude-switcher/releases/tag/v1.0.0
