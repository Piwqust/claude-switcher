<#
.SYNOPSIS
    Regenerates every picture in docs\images from the current build.

.DESCRIPTION
    1. Starts the demo switcher (tools\demo.ps1) at 200% in both themes and both languages
       and captures its windows (tools\capture.ps1). Only stand-in accounts are shown, and
       only the switcher's own windows are captured, never the screen.
    2. Writes the icon samples with --icon-samples.
    3. Lays the captures out with tools\compose.html and renders each layout with headless
       Edge at 2x, trimming the transparent margin.

    English pictures go to docs\images, Russian ones to docs\images\ru. Takes a couple of
    minutes. Build first: build.cmd. The captures stay in %TEMP%\cps-images for the
    animated hero and the social preview: run tools\hero\render.ps1 next.

.EXAMPLE
    .\tools\make-images.ps1
    .\tools\make-images.ps1 -Languages en
#>
param(
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\x64\ClaudeProfileSwitcher.exe'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\docs\images'),
    [string[]]$Languages = @('en', 'ru')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing, PresentationCore, WindowsBase
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System.Drawing; using System.Drawing.Imaging;
public static class TrimImage {
    public static Rectangle Bounds(Bitmap b) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var px = new byte[d.Stride * b.Height]; System.Runtime.InteropServices.Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d);
        int x0 = b.Width, y0 = b.Height, x1 = -1, y1 = -1;
        for (int y = 0; y < b.Height; y++) for (int x = 0; x < b.Width; x++)
            if (px[y * d.Stride + x * 4 + 3] > 2) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
        return new Rectangle(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    }
}
"@
$Exe = (Resolve-Path $Exe).Path
$OutDir = [IO.Path]::GetFullPath($OutDir)
$root = Join-Path $env:TEMP 'cps-images'          # short on purpose: the demo folder must stay under MAX_PATH
if (Test-Path $root) { Remove-Item $root -Recurse -Force }
$edge = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe"

function Render($work, $view, $lang, $size, $out, $factor = 2, [switch]$Trim) {
    $shot = "$work\out.png"; if (Test-Path $shot) { Remove-Item $shot }
    $url = "file:///$($work -replace '\\', '/')/compose.html#$view&lang=$lang"
    Start-Process $edge -Wait -WindowStyle Hidden -ArgumentList '--headless=new', '--disable-gpu', '--hide-scrollbars',
        "--force-device-scale-factor=$factor", '--default-background-color=00000000', "--window-size=$size",
        "--screenshot=$shot", "--user-data-dir=$work\edge", '--virtual-time-budget=1500', $url
    $img = [System.Drawing.Bitmap]::FromFile($shot)
    if ($Trim) { $c = $img.Clone([TrimImage]::Bounds($img), $img.PixelFormat); $img.Dispose(); $img = $c }
    New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
    $img.Save($out); "Wrote $out ($($img.Width)x$($img.Height))"; $img.Dispose()
}

foreach ($lang in $Languages) {
    $work = Join-Path $root $lang
    New-Item -ItemType Directory -Force "$work\ico-src", "$work\ico" | Out-Null
    Copy-Item "$PSScriptRoot\compose.html" $work

    # ---- icons: the samples plus the demo's own accounts, named in this language
    & "$PSScriptRoot\demo.ps1" -Exe $Exe -Language $lang | Out-Null; Start-Sleep 1
    Get-Process ClaudeProfileSwitcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force
    $env:CPS_DEMO = Join-Path $env:TEMP 'cps-demo'; $env:CPS_LANG = $lang
    Start-Process $Exe -ArgumentList '--icon-samples', "$work\ico-src" -Wait
    Remove-Item Env:CPS_DEMO, Env:CPS_LANG
    foreach ($ico in Get-ChildItem "$work\ico-src\*.ico") {
        $dec = New-Object System.Windows.Media.Imaging.IconBitmapDecoder((New-Object Uri($ico.FullName)), 'None', 'OnLoad')
        foreach ($size in 16, 32, 64, 256) {
            $frame = $dec.Frames | Where-Object { $_.PixelWidth -eq $size } | Select-Object -First 1
            $enc = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
            $enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($frame))
            $name = if ($size -eq 256) { "$($ico.BaseName).png" } else { "$($ico.BaseName)-$size.png" }
            $fs = [IO.File]::Create("$work\ico\$name"); $enc.Save($fs); $fs.Close()
        }
    }

    # ---- 1. window captures at 200%
    foreach ($theme in 'dark', 'light') {
        $common = @{ Exe = $Exe; Scale = 2 }
        & "$PSScriptRoot\demo.ps1" @common -Theme $theme -Language $lang -KeepFlyout | Out-Null; Start-Sleep 2
        & "$PSScriptRoot\capture.ps1" @common -Window picker -ClientOnly -Out "$work\picker-$theme.png" | Out-Null
        & "$PSScriptRoot\capture.ps1" @common -Window settings -ClickSettings -ClientOnly -Out "$work\settings-$theme.png" | Out-Null
        & "$PSScriptRoot\capture.ps1" @common -Window flyout -OpenFlyout -Out "$work\flyout-$theme.png" | Out-Null
        & "$PSScriptRoot\demo.ps1" @common -Theme $theme -Language $lang -Startup | Out-Null; Start-Sleep -Milliseconds 1500
        & "$PSScriptRoot\capture.ps1" @common -Window picker -ClientOnly -Out "$work\startup-$theme.png" | Out-Null
    }
    Get-Process ClaudeProfileSwitcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force

    # ---- 2. layouts
    $dest = if ($lang -eq 'en') { $OutDir } else { Join-Path $OutDir $lang }
    foreach ($v in @(
        @('picker-dark', '900,620', 'picker-dark'), @('picker-light', '900,620', 'picker-light'),
        @('startup-dark', '900,700', 'startup-dark'), @('startup-light', '900,700', 'startup-light'),
        @('tray', '1000,700', 'tray'), @('themes', '900,620', 'themes'),
        @('settings', '1500,900', 'settings'), @('icons', '1100,560', 'icons'))) {
        Render $work $v[0] $lang $v[1] "$dest\$($v[2]).png" -Trim
    }
    if ($lang -eq 'en') { Copy-Item "$work\ico\app.png" (Join-Path $OutDir 'logo.png') -Force }
}
