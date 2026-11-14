# Security

## Supported versions

Fixes ship in the latest version. Please update to it before reporting a problem.

## Reporting a vulnerability

Please do not open a public issue. Use GitHub's [private vulnerability reporting](https://github.com/Piwqust/claude-switcher/security/advisories/new) instead.

Describe what the vulnerability allows, how to reproduce it and which version you used. You will get a reply within a week. Once it is fixed, a new version is released and its notes credit you, unless you would rather they did not.

## What the program does and does not do

- Desktop-only switching makes no network requests and does not read sign-in credentials.
- The update check asks `api.github.com` for this repository's latest release at start and every 6 hours (Settings → About can turn it off). Nothing about you or your accounts is sent. An update is downloaded from that release over HTTPS, installed only when its SHA-256 matches the release's `SHA256SUMS.txt`, and only when you click **Update**.
- The optional **Use for Claude Code** action decrypts the selected Desktop profile's access token with the current Windows user's secure storage. It sends that token only over HTTPS to `api.anthropic.com/api/oauth/profile`, with redirects disabled, to verify the account. No model requests are made by the switcher.
- The access token is stored in `env.CLAUDE_CODE_OAUTH_TOKEN` in `%USERPROFILE%\.claude\settings.json`, as required for ordinary Claude Code launches. The file and temporary replacements are restricted to the current user and Windows SYSTEM. Treat this settings file as a credential file; do not share it or commit it. No refresh token is exported or refreshed by the switcher; Desktop remains its owner.
- While running, the switcher checks the selected profile every 15 seconds and copies a changed, verified access token. It does not replace explicit credentials configured elsewhere. Project settings, managed settings, custom configuration directories, bare mode and explicit environment overrides may select their own credentials.
- **Restore previous login** removes only the switcher's token setting. The existing `.credentials.json`, account metadata, MCP logins and other settings are never rewritten. `%LOCALAPPDATA%\ClaudeProfiles\.switcher\code-auth.json` records the selection and token fingerprints for crash recovery, without raw tokens.
- It does not modify Claude Desktop's installation or profile files.
- It runs with your user's rights and writes to `%LOCALAPPDATA%\ClaudeProfiles`, to the optional Claude Code user settings described above, to shortcuts on the desktop and in the Start menu, and to the `Claude Profile Switcher` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
- It reads the command lines of `claude.exe` processes to tell which accounts are open.

## Verifying a download

Releases are built by GitHub Actions from this repository, and every release has a `SHA256SUMS.txt` next to its files. Compare the hash of what you downloaded:

```powershell
Get-FileHash .\ClaudeProfileSwitcher-x64.exe -Algorithm SHA256
```
