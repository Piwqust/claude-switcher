# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [Semantic Versioning](https://semver.org/).
Each version's section becomes the description of its GitHub release.

## [Unreleased]

Most of these fixes come from [@z6gg](https://github.com/z6gg) in [#6](https://github.com/Piwqust/claude-switcher/pull/6). Thank you!

### Fixed

- **Deleting an account removes all of it.** Claude unpacks files into each profile with paths longer than Windows' classic 260-character limit, which the old delete couldn't remove: it stopped half way, said nothing, and the account stayed. Profiles are now deleted with long-path support, and the folder is moved aside first, so a delete either completes or touches nothing.
- Deleting an open account closes its Claude window and Claude Code sessions first instead of refusing. If another program still holds a file, the message names it. The delete runs in the background with a "Deleting…" status, clears the account's colour and auto-open entries and removes its icon files, which used to pile up. If it was the Claude Code default, Code gets its previous login back.
- **Browser sign-ins work again for people who used the PowerShell version.** Windows kept sending `claude://` links to that version's handler, which no longer exists, so a sign-in finished in the browser never came back. The switcher now takes that handler over, so the choice already made in Windows Settings keeps working.
- **The main account is found where current Claude keeps it.** Store builds since 2.42304 move its data to `%LOCALAPPDATA%\Claude-Data`; the switcher still looked in the old folder, so Main showed no usage or last-used time and chats couldn't be moved to or from it.
- A Claude update is picked up at once, instead of the switcher starting the previous version while its folder lingers.
- Opening an account that is already open brings its window forward instead of only flashing its taskbar button.
- Adding an account asks about `claude://` links every time until they're set up, not just once. A failed add writes its reason to the error log.
- Quitting while a dialog is open quits.
- Account names Windows reserves (COM5 to COM9, LPT4 to LPT9, CONIN$, CONOUT$) are refused like the others.

### Changed

- The error log is appended to instead of rewritten on every line, and starts over past 256 KB.
- `settings.json` and Claude Code's large `.claude.json` are parsed again only when they change, instead of on every lookup and refresh.

## [1.1.1] — 2026-11-14

### Fixed

- The tray list no longer opens partly under the taskbar the first time after the switcher starts or updates. It is now measured before it is placed, and stays against the taskbar if its height changes while open. Dialogs and the main window are centred by their real size too.
- Settings → About says "Checked today, 13:47" instead of "Checked Today, 13:47".

## [1.1.0] — 2026-11-14

### New

- **The switcher updates itself.** When a new version is out, an **Update** button appears in the tray flyout, the tray menu and Settings → About. One click: the new exe is downloaded, checked against the release's SHA-256 checksum and swapped in, in a few seconds. Claude's windows stay open.
- It looks for a new release at start and every 6 hours, asking GitHub only for the latest version number. Turn this off in Settings → About; **Check now** still works.
- Every release now opens with its own picture and download buttons.

## [1.0.4] — 2026-11-14

### New

- **Use a Desktop account as the default for Claude Code**, from its card's ⋯ menu or the tray menu. Ordinary `claude` launches use that account without another login. The original CLI login is kept and can be restored; the switcher follows Desktop's access-token updates while running without copying its refresh token.
- Double-click the tray icon to open the main window.

### Fixed

- The tray flyout no longer loses its right side (the open and settings buttons, shortcut digits, close buttons) on a display scaled above 100%. Its width now follows the display's scale the way its height did.
- Prevent a crash after selecting a card-menu command while the account list refreshes. Menus and confirmation dialogs retain their account IDs, and tray commands stay bound to the accounts originally shown.
- Reject incomplete or ambiguous JSON before changing settings, and preserve existing numeric values exactly when rewriting them.

## [1.0.3] — 2026-10-27

### Fixed

- **Signing in to an extra account with Google or single sign-on now comes back to that account**, not to the main one. Windows sends `claude://` links to the Store package, so the switcher can now handle them instead: choose it once in Settings → Sign-in and links (adding an account offers this too), and each link goes to the account whose window you used last.
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

[Unreleased]: https://github.com/Piwqust/claude-switcher/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/Piwqust/claude-switcher/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/Piwqust/claude-switcher/compare/v1.0.4...v1.1.0
[1.0.4]: https://github.com/Piwqust/claude-switcher/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/Piwqust/claude-switcher/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/Piwqust/claude-switcher/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/Piwqust/claude-switcher/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/Piwqust/claude-switcher/releases/tag/v1.0.0
