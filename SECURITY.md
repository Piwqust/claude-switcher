# Security

## Supported versions

Fixes ship in the latest version. Please update to it before reporting a problem.

## Reporting a vulnerability

Please do not open a public issue. Use GitHub's [private vulnerability reporting](https://github.com/Piwqust/claude-profile-switcher/security/advisories/new) instead.

Describe what the vulnerability allows, how to reproduce it and which version you used. You will get a reply within a week. Once it is fixed, a new version is released and its notes credit you, unless you would rather they did not.

## What the program does and does not do

- It makes no network requests.
- It does not read or copy tokens, cookies or passwords.
- It does not modify Claude's files.
- It runs with your user's rights and writes only to `%LOCALAPPDATA%\ClaudeProfiles`, to shortcuts on the desktop and in the Start menu, and to the `Claude Profile Switcher` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- It reads the command lines of `claude.exe` processes to tell which accounts are open.

## Verifying a download

Releases are built by GitHub Actions from this repository, and every release has a `SHA256SUMS.txt` next to its files. Compare the hash of what you downloaded:

```powershell
Get-FileHash .\ClaudeProfileSwitcher-x64.exe -Algorithm SHA256
```
