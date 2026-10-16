<#
.SYNOPSIS
    Opens the switcher over stand-in accounts, for screenshots and trying out the UI.

.DESCRIPTION
    Creates a throwaway folder with three made-up accounts (labels, last-used times, plan
    usage, two of them "open") and starts a separate switcher on it through CPS_DEMO.
    The demo never starts Claude and does not touch your real accounts, shortcuts,
    autostart, hotkeys, jump list or the switcher you normally run.

.EXAMPLE
    .\tools\demo.ps1
    .\tools\demo.ps1 -Theme light -Language ru
    .\tools\demo.ps1 -Scale 2          # every window at 200%, for sharp screenshots
#>
param(
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\x64\ClaudeProfileSwitcher.exe'),
    [string]$Folder = (Join-Path $env:TEMP 'cps-demo'),   # keep it short: paths must stay under MAX_PATH
    [ValidateSet('system', 'dark', 'light')][string]$Theme = 'system',
    [ValidateSet('en', 'ru', 'settings')][string]$Language = 'en',   # settings: whatever the Language setting says
    [ValidateRange(1, 4)][double]$Scale = 0,               # 0: follow the monitor
    [switch]$Tray,
    [switch]$Startup,       # as at sign-in: with the countdown to the account set to open
    [switch]$KeepFlyout     # the tray flyout stays open when it loses focus, for screenshots
)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path

# A demo switcher may still be running from last time. The installed switcher runs from
# %LOCALAPPDATA%\ClaudeProfiles\.switcher, so it never matches this path.
Get-Process ClaudeProfileSwitcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force
Start-Sleep -Milliseconds 300
if (Test-Path $Folder) { Remove-Item $Folder -Recurse -Force }

$utf8 = New-Object System.Text.UTF8Encoding $true
function Write-Json($path, $value) {
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    [IO.File]::WriteAllText($path, ($value | ConvertTo-Json -Depth 6), $utf8)
}

$names = if ($Language -eq 'ru') { @('Работа', 'Личный') } else { @('Work', 'Personal') }
$now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$accounts = @(
    @{ Id = '.default';  Label = $null;     Used = 0;  Five = 34; Week = 61 },
    @{ Id = 'Account 2'; Label = $names[0]; Used = 0;  Five = 72; Week = 48 },
    @{ Id = 'Account 3'; Label = $names[1]; Used = 26; Five = 12; Week = 23 }
)
$labels = @{}
foreach ($a in $accounts) {
    $dir = Join-Path $Folder $a.Id
    New-Item -ItemType Directory -Force $dir | Out-Null
    # "Last used" comes from the files Electron rewrites while running.
    $prefs = Join-Path $dir 'Preferences'
    Set-Content $prefs '{}'
    (Get-Item $prefs).LastWriteTime = (Get-Date).AddHours(-$a.Used)
    if ($null -ne $a.Five) {
        Write-Json (Join-Path $dir 'plan-usage-history.json') @{ samples = @(@{ t = $now - 60000; u = @{ fh = $a.Five; sd = $a.Week } }) }
    }
    if ($a.Label) { $labels[$a.Id] = $a.Label }
}

Write-Json (Join-Path $Folder 'settings.json') @{
    Labels       = $labels
    DemoRunning  = @('Default', 'Account 2')
    AutoOpen     = @('Account 2')
    Autostart    = $true
    AskAtStartup = $true
}

function Set-Env($name, $value) { if ($value) { Set-Item "Env:$name" $value } else { Remove-Item "Env:$name" -ErrorAction SilentlyContinue } }
Set-Env CPS_DEMO $Folder
Set-Env CPS_LANG $(if ($Language -ne 'settings') { $Language })
Set-Env CPS_THEME $(if ($Theme -ne 'system') { $Theme })
Set-Env CPS_SCALE $(if ($Scale) { "$Scale" })
Set-Env CPS_FLYOUT_STAY $(if ($KeepFlyout) { '1' })

$start = @{ FilePath = $Exe; WorkingDirectory = (Split-Path $Exe) }
if ($Tray) { $start.ArgumentList = '--tray' } elseif ($Startup) { $start.ArgumentList = '--startup' }
Start-Process @start
"Demo switcher started over $Folder"
