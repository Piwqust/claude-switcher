# User guide

Everything Claude Profile Switcher can do, in the order you are likely to need it. For the short version, see the [README](../README.md). For what happens under the hood, see [How it works](how-it-works.md).

<sub>[Русский](guide.ru.md)</sub>

- [Install](#install)
- [The picker](#the-picker)
- [At Windows sign-in](#at-windows-sign-in)
- [Tray](#tray)
- [Taskbar and shortcuts](#taskbar-and-shortcuts)
- [Settings](#settings)
- [Moving Claude Code chats](#moving-claude-code-chats)
- [Usage on the cards](#usage-on-the-cards)
- [Command line](#command-line)
- [Troubleshooting](#troubleshooting)
- [Uninstall](#uninstall)

## Install

1. Download the exe for your processor:

   | Processor | File |
   | --- | --- |
   | Intel or AMD (almost every PC) | [ClaudeProfileSwitcher-x64.exe](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-x64.exe) |
   | ARM: Snapdragon, Surface Pro X and the like | [ClaudeProfileSwitcher-arm64.exe](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-arm64.exe) |

   Not sure which you have? Open **Settings → System → About** and look at **System type**.

2. Run it. It installs itself into `%LOCALAPPDATA%\ClaudeProfiles\.switcher`, adds shortcuts to the desktop and the Start menu, turns on start with Windows and opens the picker. No admin rights needed.

3. You can delete the downloaded file afterwards; the installed copy is what runs from then on.

> [!NOTE]
> The exe is not code-signed, so the first time you run it Windows may say "Windows protected your PC". Click **More info → Run anyway**. It happens once: the installed copy does not trigger it. To make sure the file is the one built here, compare its hash with `SHA256SUMS.txt` in the release:
>
> ```powershell
> Get-FileHash .\ClaudeProfileSwitcher-x64.exe -Algorithm SHA256
> ```

**You need** Windows 10 or 11 and [Claude Desktop](https://claude.ai/download), from the Microsoft Store or the installer. The switcher finds it on its own.

**Updating:** download the new version and run it. It closes the old one, takes its place and keeps every account, name and setting. Upgrading from the PowerShell switcher (v2) works the same way.

## The picker

<picture><source media="(prefers-color-scheme: light)" srcset="images/picker-light.png"><img src="images/picker-dark.png" alt="The picker with three accounts" width="560" align="right"></picture>

Each card shows the account's number, its name, whether it is open or when it was last used, and its usage when there is a current figure. A green dot means the account's window is already open: clicking it brings that window forward rather than starting a second copy.

**Adding your first extra account:**

1. Click **Add**. A new Claude window opens at the sign-in screen.
2. Sign in with your other account. From now on both stay signed in.
3. Press <kbd>F2</kbd> to rename `Account 2` to something like "Work". Only the label changes; the sign-in is unaffected.

The **⋯** menu on a card (or a right-click) has the rest: open it when Windows starts, a desktop shortcut, moving Code chats, the profile folder, and delete.

<br clear="right">

| Action | Mouse | Keyboard |
| --- | --- | --- |
| Open an account, or switch to its window | click the card | <kbd>1</kbd>…<kbd>9</kbd> or <kbd>Enter</kbd> |
| Pick several and open them together | <kbd>Ctrl</kbd>+click | <kbd>Space</kbd>, then <kbd>Enter</kbd> |
| Add an account | **Add** | <kbd>Ctrl</kbd>+<kbd>N</kbd> |
| Rename | ⋯ → Rename | <kbd>F2</kbd> |
| Delete an account | ⋯ → Delete | <kbd>Del</kbd> |
| Card menu | right-click or ⋯ | <kbd>Shift</kbd>+<kbd>F10</kbd> |
| Settings | ⚙ | <kbd>Ctrl</kbd>+<kbd>,</kbd> |
| Close the window | ✕ | <kbd>Esc</kbd> |
| Move between cards | | <kbd>Tab</kbd> and the arrow keys |

From any app: <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd> for the picker, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>…<kbd>9</kbd> for an account directly. You can change the picker's shortcut in Settings; if another app already uses it, Settings says so.

<p align="center">
  <img src="images/themes.png" alt="The account picker, half in the dark theme and half in the light one" width="720">
</p>

## At Windows sign-in

<picture><source media="(prefers-color-scheme: light)" srcset="images/startup-light.png"><img src="images/startup-dark.png" alt="A countdown before the Work account opens" width="520" align="right"></picture>

The switcher starts with Windows and then does one of two things:

- **Asks which account to open.** The picker appears as soon as you sign in.
- **Opens the ticked accounts on its own.** Tick them in a card's menu ("Open when Windows starts") or in Settings. A 3, 5 or 10 second countdown runs first; any key or click stops it so you can choose something else.

With **Ask at sign-in** off, the ticked accounts open without asking.

<br clear="right">

## Tray

<img src="images/tray.png" alt="The tray flyout in the dark and light themes" width="520" align="right">

Closing the window leaves the switcher in the tray, where it uses about 20 MB of memory and opens instantly.

- **Left-click** the icon for the account list with usage.
- **Right-click** it for the standard Windows menu: accounts, Add account…, Settings, Quit switcher.

If you would rather it quit with the window, turn off **Stay in the tray when closed** in Settings.

<br clear="right">

## Taskbar and shortcuts

- **Jump list.** Pin the switcher to the taskbar and right-click it to see every account.
- **A button per account.** Windows normally groups every Claude window under one button. Turn on **Separate taskbar buttons per account** in Settings and each window gets its own button and icon. This one is experimental.
- **Shortcuts.** **Desktop shortcut** in a card's menu creates `Claude - <name>`. Pin it to Start or the taskbar to open that account without the picker. Shortcuts keep working after Claude updates itself.

<p align="center">
  <img src="images/icons.png" alt="Icons: the switcher, the main account, accounts with initials, and Add account" width="760">
</p>

## Settings

<p align="center">
  <img src="images/settings.png" alt="Settings in the dark and light themes" width="100%">
</p>

Start with Windows, asking at sign-in, which accounts open on their own, the countdown, shortcuts, hiding the picker after opening an account, the tray, separate taskbar buttons and the interface language.

## Moving Claude Code chats

Card menu → **Move Code chats…**. Tick the chats and choose the account to copy them to. The chat stays in the original account too. Both copies carry on the same history, so finish in one account before picking it up in the other.

> [!NOTE]
> The account receiving the chats must have opened the Code tab at least once. If it is open, restart it to see them.

## Usage on the cards

The cards show how much of the 5-hour and 7-day limits each account has used. A figure is shown only if it is at most 15 minutes old and its window has not reset since; anything older is hidden, because the account may have been used in the browser or the terminal in the meantime. With no current figure, the meters are simply hidden.

The switcher never asks Anthropic's servers for these numbers. Where they come from is in [How it works](how-it-works.md#where-the-usage-figures-come-from).

## Command line

| Command | Does |
| --- | --- |
| `ClaudeProfileSwitcher.exe` | Opens the picker. Run from anywhere but its install folder, it installs itself first |
| `ClaudeProfileSwitcher.exe --launch "Work"` | Opens an account, or brings its window forward, then exits. Takes a name or a profile folder |
| `ClaudeProfileSwitcher.exe --add-account` | Adds an account and opens Claude at its sign-in screen |
| `ClaudeProfileSwitcher.exe --tray` | Starts in the tray only |
| `ClaudeProfileSwitcher.exe --startup` | What Windows runs at sign-in: the countdown or the picker |
| `ClaudeProfileSwitcher.exe --install` | Installs or refreshes the install, then opens the picker |
| `ClaudeProfileSwitcher.exe --claude-path "<dir>\Claude.exe"` | Points at Claude if it cannot be found. The choice is remembered |

## Troubleshooting

<details>
<summary><b>"Couldn't find the Claude app on this PC"</b></summary>

The switcher offers to let you pick `Claude.exe` yourself and remembers the choice. `--claude-path` does the same.
</details>

<details>
<summary><b>The global shortcut does nothing</b></summary>

Most likely another app already uses it. Settings then says "Another app already uses this shortcut"; choose a different one there.
</details>

<details>
<summary><b>No usage on the cards</b></summary>

There is no current figure: Claude has not written one yet, or it is older than 15 minutes. Open the account and use it for a while; the meters appear when Claude updates them.
</details>

<details>
<summary><b>A desktop shortcut does not open anything</b></summary>

Errors are shown in a window and written to `%LOCALAPPDATA%\ClaudeProfiles\switcher-error.log`. If a profile was deleted or renamed outside the switcher, its old shortcut says there is no such account.
</details>

<details>
<summary><b>The interface is in the wrong language</b></summary>

By default it follows the Windows display language: Russian when Windows is in Russian, English otherwise. Settings → Language → **English** or **Русский** pins one.
</details>

## Uninstall

1. In Settings, turn off **Start with Windows**, then choose **Quit switcher** in the tray.
2. Delete `%LOCALAPPDATA%\ClaudeProfiles\.switcher` and the `Claude Profile Switcher` and `Claude - …` shortcuts on the desktop and in the Start menu.
3. To remove the extra accounts and their sign-ins too, delete all of `%LOCALAPPDATA%\ClaudeProfiles`. Your main Claude account is unaffected.
