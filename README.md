<div align="center">

<picture>
  <source media="(prefers-reduced-motion: reduce)" srcset="docs/images/hero.png">
  <source srcset="docs/images/hero.avif" type="image/avif">
  <source srcset="docs/images/hero.webp" type="image/webp">
  <img src="docs/images/hero.gif" alt="Press Ctrl+Alt+C, pick an account in Claude Profile Switcher, and it opens already signed in" width="100%">
</picture>

# Claude Profile Switcher

**Several Claude Desktop accounts on one Windows PC: each in its own window, all signed in at once.**

[![Latest release](https://img.shields.io/github/v/release/Piwqust/claude-switcher?style=flat-square&color=d9704c)](https://github.com/Piwqust/claude-switcher/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Piwqust/claude-switcher/total?style=flat-square&color=d9704c)](https://github.com/Piwqust/claude-switcher/releases)
[![MIT License](https://img.shields.io/github/license/Piwqust/claude-switcher?style=flat-square)](LICENSE)

<a href="https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-x64.exe"><img src="docs/images/download-x64.svg" alt="Download for Windows x64" height="52"></a>
&nbsp;
<a href="https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-arm64.exe"><img src="docs/images/download-arm64.svg" alt="Download for Windows ARM64" height="52"></a>

<sub>One exe, nothing else to install · <a href="https://claude-switcher.angleford.co">website</a> · <a href="https://github.com/Piwqust/claude-switcher/releases">all releases</a> · <a href="docs/guide.md">user guide</a> · <a href="README.ru.md">Русский</a></sub>

</div>

Claude Desktop holds one sign-in at a time, so switching to your work account means signing out of your personal one. Claude Profile Switcher gives every account its own window with its own sign-in, chats and settings, and lets you have all of them open at once.

- **One shortcut.** <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd> opens the picker from any app, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>1</kbd>…<kbd>9</kbd> opens an account directly.
- **Always at hand.** It waits in the tray, starts with Windows and can open your usual accounts on its own.
- **Usage at a glance.** Each card shows how much of the 5-hour and 7-day limits that account has used.
- **Your main account stays as it is**, with auto update and `claude://` links.
- **Small and native.** One 550 KB exe in C++, no .NET or Electron, English and Russian, dark and light themes.

## Install

1. Download the exe: [x64](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-x64.exe) for Intel and AMD (almost every PC) or [ARM64](https://github.com/Piwqust/claude-switcher/releases/latest/download/ClaudeProfileSwitcher-arm64.exe) for Snapdragon.
2. Run it. It installs itself, without admin rights, and opens the picker.
3. Click **Add** and sign in with your other account in the new Claude window. That's it: both stay signed in.

You need Windows 10 or 11 and [Claude Desktop](https://claude.ai/download). The exe is not code-signed, so the first time Windows may say "Windows protected your PC": click **More info → Run anyway**. To update, run the new version; accounts and settings are kept.

## Using it

| To | Do |
| --- | --- |
| Open an account | click its card or press <kbd>1</kbd>…<kbd>9</kbd> |
| Open several at once | <kbd>Ctrl</kbd>+click the cards, then **Open 2 accounts** |
| Rename or delete | <kbd>F2</kbd> or <kbd>Del</kbd>, or the **⋯** menu on a card |
| Get to it from any app | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>C</kbd>, or click the tray icon |

A green dot on a card means that account is already open, and clicking it brings its window forward. The countdown at Windows sign-in, taskbar buttons per account, moving Claude Code chats between accounts, the command line and troubleshooting are all in the **[user guide](docs/guide.md)**.

## How it works

Claude Desktop is an Electron app, and an Electron app keeps everything in one data folder. Started with `--user-data-dir` pointing at another folder, it is a separate sign-in that runs next to the first one. The switcher keeps those folders in `%LOCALAPPDATA%\ClaudeProfiles`, starts Claude with the right one and watches which are open. Your main account starts the usual way and is never touched.

It makes no network requests and never reads tokens, cookies or passwords. What is shared between accounts, where the usage figures come from and the rest: **[How it works](docs/how-it-works.md)**.

## Compared with the original

This is a C++ rewrite of [Claude Code Desktop Switcher](https://github.com/PriyanshuGeTRekT/Claude-Code-Desktop-Switcher) by [@PriyanshuGeTRekT](https://github.com/PriyanshuGeTRekT), the PowerShell script that worked out the idea. The profile folders are the same, so an existing setup carries straight over.

| | Original 2.0 | This 1.0 |
| --- | --- | --- |
| Runs as | a PowerShell script | one 550 KB exe |
| Window appears in | about 10 s | about 75 ms |
| Memory | about 126 MB | about 16 MB |
| New here | | shortcuts, tray flyout, usage, several at once, sign-in countdown, Russian, dark theme, ARM64 |

The full comparison and why it is faster: [How it works → Compared with the original](docs/how-it-works.md#compared-with-the-original).

## Building from source

Visual Studio 2022 or the Build Tools with "Desktop development with C++", then `build.cmd` (or `build.cmd arm64`). The demo over made-up accounts and what to check before a pull request are in [CONTRIBUTING.md](CONTRIBUTING.md).

---

<sub>Unofficial: not affiliated with, endorsed by or supported by Anthropic. Claude is a trademark of Anthropic. Using several accounts is subject to [Anthropic's terms](https://www.anthropic.com/legal/consumer-terms). Thanks to @PriyanshuGeTRekT for the original. [MIT license](LICENSE).</sub>
