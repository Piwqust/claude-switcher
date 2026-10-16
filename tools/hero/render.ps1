<#
.SYNOPSIS
    Renders the animated README hero (docs\images\hero.*) with Remotion.

.DESCRIPTION
    Uses the window captures that tools\make-images.ps1 leaves in %TEMP%\cps-images\<lang>,
    so run that first. For each language it renders the loop at 2x and encodes:

      hero.avif   the animation most browsers load (a few hundred KB)
      hero.webp   the animation for browsers without AVIF
      hero.gif    960 px, 20 fps, the last fallback
      hero.png    one still frame, for "reduce motion"

    and, for English, social-preview.png (GitHub's 1280x640, under 1 MB).

    Needs Node.js, ffmpeg on PATH and Microsoft Edge. The first run installs the
    packages in tools\hero. Preview and edit the animation with: npx remotion studio

.EXAMPLE
    .\tools\hero\render.ps1
    .\tools\hero\render.ps1 -Languages en
#>
param(
    [string[]]$Languages = @('en', 'ru'),
    [string]$Captures = (Join-Path $env:TEMP 'cps-images'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\..\docs\images'),
    [string]$Browser = "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
    [int]$StillFrame = 115
)
$ErrorActionPreference = 'Stop'
$OutDir = [IO.Path]::GetFullPath($OutDir)

function Check($what) { if ($LASTEXITCODE) { throw "$what failed ($LASTEXITCODE)" } }

Push-Location $PSScriptRoot
try {
    # npx.cmd, not npx: the npx.ps1 shim mangles arguments that contain spaces.
    if (-not (Test-Path node_modules)) { npm.cmd ci; Check 'npm ci' }
    # Remotion can download its own headless Chrome, but the download fails on long paths; Edge is already here.
    $browserArg = if (Test-Path $Browser) { @("--browser-executable=$Browser") } else { @() }

    foreach ($lang in $Languages) {
        $src = Join-Path $Captures $lang
        if (-not (Test-Path "$src\picker-dark.png")) { throw "No captures in $src. Run tools\make-images.ps1 first." }
        $pub = "public\$lang"
        New-Item -ItemType Directory -Force "$pub\ico" | Out-Null
        Copy-Item "$src\picker-dark.png" $pub -Force
        foreach ($ico in 'app-32', 'app-64', 'account-1', 'account-2', 'account-3') { Copy-Item "$src\ico\$ico.png" "$pub\ico" -Force }

        $frames = "out\$lang\frames"
        if (Test-Path $frames) { Remove-Item $frames -Recurse -Force }
        npx.cmd remotion render "hero-$lang" $frames --sequence --image-format=png --scale=2 --concurrency=4 --log=error $browserArg
        Check 'remotion render'
        npx.cmd remotion still "hero-$lang" "out\$lang\still.png" "--frame=$StillFrame" --scale=2 --log=error $browserArg
        Check 'remotion still'

        $dest = if ($lang -eq 'en') { $OutDir } else { Join-Path $OutDir $lang }
        New-Item -ItemType Directory -Force $dest | Out-Null
        $in = @('-v', 'error', '-y', '-framerate', '25', '-i', "$frames\element-%03d.png")
        $full = 'scale=1280:-1:flags=lanczos'
        ffmpeg @in -vf "$full,format=yuv420p" -c:v libaom-av1 -crf 32 -cpu-used 4 -row-mt 1 -loop 0 -f avif "$dest\hero.avif"
        Check 'ffmpeg avif'
        ffmpeg @in -vf $full -c:v libwebp_anim -quality 75 -compression_level 6 -loop 0 "$dest\hero.webp"
        Check 'ffmpeg webp'
        # One palette for the whole loop and ordered dithering keep the unchanged parts of each frame identical, which is what GIF compresses.
        ffmpeg @in -vf 'fps=20,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=stats_mode=full[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle' -loop 0 "$dest\hero.gif"
        Check 'ffmpeg gif'
        ffmpeg -v error -y -i "out\$lang\still.png" -vf $full "$dest\hero.png"
        Check 'ffmpeg png'
        if ($lang -eq 'en') { Copy-Item "$dest\hero.png" (Join-Path $OutDir 'social-preview.png') -Force }

        Get-ChildItem "$dest\hero.*" | ForEach-Object { 'Wrote {0} ({1:N0} KB)' -f $_.FullName, ($_.Length / 1KB) }
    }
}
finally { Pop-Location }
