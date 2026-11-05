# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [Semantic Versioning](https://semver.org/).
Each version's section becomes the description of its GitHub release.

## [Unreleased]

### Added

- **A new account names itself.** Once you sign in, its card takes the name the account goes by in Claude instead of staying "Account 2". It happens once; renaming it yourself (F2) always wins. Only the display name is read, from Claude's local cache in that profile.

### Fixed

- **Deleting an account works.** Claude unpacks files into each profile with paths longer than Windows' classic 260-character limit, which the shell's delete can't remove: it stopped half way, said nothing, and the account stayed. Profiles are now deleted with long-path support, and the folder is moved aside first, so a delete either completes or touches nothing.
- Deleting an open account closes its Claude window first instead of refusing, and also ends anything still running from inside its folder, such as Claude Code. If another program still holds a file, the message names it.
- Deleting runs in the background with a "Deleting…" status instead of freezing the window, and also clears the account's colour, auto-open and auto-name entries. Its icon files are now removed too; they used to pile up.
- Quitting while a dialog was open did nothing; it now quits.
- Account names Windows reserves (COM5 to COM9, LPT4 to LPT9, CONIN$, CONOUT$) are refused like the others.
- The About card shows the real version instead of a fixed "1.0".
- **Signing in to an added account works again for people who used the PowerShell version.** Windows kept sending `claude://` links, the end of every browser sign-in, to the PowerShell version's handler, which no longer exists, so the sign-in never came back. The switcher now takes that handler over and points it at itself, so the choice already made in Windows Settings keeps working.
- **The main account is found where current Claude keeps it.** Claude 4.x moved its data to `%LOCALAPPDATA%\Claude-Data`; the switcher still looked in the old, now empty folder, so Main showed no usage or last-used time and chats couldn't be moved to or from it. Whichever known folder Claude wrote to last is used now.
- **A Claude update is picked up at once.** The Store install is looked up on every start (one registry key) instead of trusting the cached path, which could start the previous version while its folder lingered.
- Bringing an open account forward no longer just flashes its taskbar button when Windows' focus lock gets in the way.
- Adding an account asks about `claude://` links every time until they're set up, instead of once ever; and a failed add now writes its reason to the error log.

### Changed

- The error log is appended to rather than rewritten on every line, and starts over past 256 KB.
- Usage files, including Claude Code's large `.claude.json`, are read only for the cards and parsed again only when they change, instead of for every name lookup and every refresh.
- `settings.json` is parsed once and re-read only when it changes, instead of on every setting lookup.

## [1.0.3] - 2026-10-27

### Fixed

- **Signing in to an extra account with Google or single sign-on now comes back to that account**, not to the main one. Windows sends `claude://` links to the Store package, so the switcher can now handle them instead: choose it once in Settings → Sign-in and links (adding an account offers this too), and each link goes to the account whose window you used last.
- Restarting or closing Claude no longer freezes Windows for a couple of seconds when Claude has many MCP servers running. The processes are now ended a few at a time instead of all at once.

## [1.0.2] - 2026-10-20

### New

- **A restart button in the tray flyout** next to every open account.
- **Close Claude for good**, tray icon included: ✕ in the tray flyout, Close Claude in the tray menu (one account or all open ones) and in a card's ⋯ menu, or `--close "Work"` on the command line.

## [1.0.1] - 2026-10-20

### New

- **Restart Claude** in one click: from the tray menu (one account or all open ones), from a card's ⋯ menu, with <kbd>Ctrl</kbd>+<kbd>R</kbd> on a card, or `--restart "Work"` on the command line. It ends the account's whole Claude process, Claude Code chats included, and opens it again.

### Fixed

- An account started with `"--user-data-dir=…"` quoted as a whole was not recognised as open.

## [1.0.0] - 2026-10-16

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

[Unreleased]: https://github.com/Piwqust/claude-switcher/compare/v1.0.3...HEAD
[1.0.3]: https://github.com/Piwqust/claude-switcher/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/Piwqust/claude-switcher/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/Piwqust/claude-switcher/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/Piwqust/claude-switcher/releases/tag/v1.0.0
