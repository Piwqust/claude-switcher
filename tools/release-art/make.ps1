# Draws docs/releases/<version>.png, the picture at the top of that GitHub release, and the
# PNG download buttons the release notes link (GitHub release notes cannot show the SVG ones).
#
#   .\tools\release-art\make.ps1 -Version 1.1.0 -Title "Updates itself" -Subtitle "One click, a few seconds" -Badge Update
#
# -Badge puts an accent button in the pictured flyout's header, for a release that adds one there.
#
# Needs Microsoft Edge. Nothing leaves the machine: Edge renders a local page and screenshots it.
param(
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$Title,
    [string]$Subtitle = '',
    [string]$Badge = ''
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edge) { throw 'Microsoft Edge was not found.' }

function Shoot([string]$url, [string]$out, [int]$w, [int]$h, [int]$Scale = 1, [switch]$Transparent) {
    $profile = Join-Path $env:TEMP "cps-release-art-$PID"
    $flags = @('--headless=new', '--disable-gpu', '--hide-scrollbars', '--allow-file-access-from-files', "--user-data-dir=$profile",
               "--window-size=$w,$h", "--force-device-scale-factor=$Scale", '--virtual-time-budget=3000', "--screenshot=$out")
    if ($Transparent) { $flags += '--default-background-color=00000000' }
    # Edge reports "bytes written" on stderr, which Windows PowerShell would treat as an error.
    $ErrorActionPreference = 'Continue'
    & $edge @flags $url 2>&1 | Out-Null
    $ErrorActionPreference = 'Stop'
    Remove-Item -Recurse -Force $profile -ErrorAction SilentlyContinue
    if (-not (Test-Path $out)) { throw "Edge did not write $out" }
}

$dir = Join-Path $root 'docs\releases'
New-Item -ItemType Directory -Force $dir | Out-Null
$page = ([Uri](Join-Path $PSScriptRoot 'art.html')).AbsoluteUri
$query = 'v=' + [Uri]::EscapeDataString($Version) + '&t=' + [Uri]::EscapeDataString($Title) + '&s=' + [Uri]::EscapeDataString($Subtitle) + '&b=' + [Uri]::EscapeDataString($Badge)
$out = Join-Path $dir "$Version.png"
Shoot "$page`?$query" $out 1600 800 -Scale 2
"Wrote $out"

# The download buttons, from the README's SVGs, at twice their size so they stay sharp.
foreach ($arch in 'x64', 'arm64') {
    $svg = Join-Path $root "docs\images\download-$arch.svg"
    $png = Join-Path $root "docs\images\download-$arch.png"
    [xml]$x = Get-Content $svg -Raw -Encoding utf8
    $w = [int][double]$x.svg.width; $h = [int][double]$x.svg.height
    $html = Join-Path $env:TEMP "cps-button-$arch.html"
    "<!doctype html><style>html,body{margin:0;background:transparent}img{display:block;width:$($w*2)px;height:$($h*2)px}</style><img src='$(([Uri]$svg).AbsoluteUri)'>" | Set-Content $html -Encoding utf8
    Shoot ([Uri]$html).AbsoluteUri $png ($w * 2) ($h * 2) -Transparent
    Remove-Item $html
    "Wrote $png"
}
