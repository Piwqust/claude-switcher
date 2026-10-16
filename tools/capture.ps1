<#
.SYNOPSIS
    Saves one of the demo switcher's windows to a PNG.

.DESCRIPTION
    Uses PrintWindow on that single window, so nothing else on the screen can end up in the
    image. -ClientOnly keeps just the window's content, without the title bar Windows draws.
    Start the demo first with tools\demo.ps1.

.EXAMPLE
    .\tools\capture.ps1 -Window picker -Out docs\images\picker.png
    .\tools\capture.ps1 -Window flyout -OpenFlyout -Out flyout.png
    .\tools\capture.ps1 -Window settings -ClickSettings -Scale 2 -ClientOnly -Out settings.png
#>
param(
    [Parameter(Mandatory)][string]$Out,
    [ValidateSet('picker', 'settings', 'flyout')][string]$Window = 'picker',
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\x64\ClaudeProfileSwitcher.exe'),
    [double]$Scale = 1,        # what the demo was started with
    [switch]$OpenFlyout,       # click the demo's tray icon first
    [switch]$ClickSettings,    # click the picker's settings button first
    [switch]$ClientOnly
)
$ErrorActionPreference = 'Stop'
$titles = @{
    picker   = @('Claude Accounts', 'Аккаунты Claude')
    settings = @('Settings — Claude Accounts', 'Настройки — Аккаунты Claude')
    flyout   = @('Account list', 'Список аккаунтов')
}
Add-Type -AssemblyName System.Drawing
Add-Type -Name W -Namespace Capture -MemberDefinition @'
public delegate bool EnumProc(IntPtr h, IntPtr l);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
[DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);
public struct RECT { public int Left, Top, Right, Bottom; }
public struct POINT { public int X, Y; }
public static IntPtr Find(uint pid, string[] titles) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => {
        uint p; GetWindowThreadProcessId(h, out p);
        if (p != pid || !IsWindowVisible(h)) return true;
        var sb = new System.Text.StringBuilder(256); GetWindowText(h, sb, 256);
        if (System.Array.IndexOf(titles, sb.ToString()) >= 0) { found = h; return false; }
        return true;
    }, IntPtr.Zero);
    return found;
}
public static IntPtr Tray(uint pid) {
    for (IntPtr h = IntPtr.Zero; (h = FindWindowEx(IntPtr.Zero, h, "ClaudeProfileSwitcherTray", null)) != IntPtr.Zero;) {
        uint p; GetWindowThreadProcessId(h, out p); if (p == pid) return h;
    }
    return IntPtr.Zero;
}
'@
$exePath = (Resolve-Path $Exe).Path
$proc = Get-Process ClaudeProfileSwitcher -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exePath } | Select-Object -First 1
if (-not $proc) { throw "The demo switcher is not running. Start it with tools\demo.ps1." }
$id = [uint32]$proc.Id

if ($OpenFlyout) {
    # NIN_SELECT, what a left click on the icon sends.
    [void][Capture.W]::PostMessage([Capture.W]::Tray($id), 0x8001, [IntPtr]::Zero, [IntPtr]0x400)
    Start-Sleep -Milliseconds 700
}
if ($ClickSettings) {
    # The gear sits 41 DIPs in from the right edge and 30 down.
    $picker = [Capture.W]::Find($id, $titles.picker)
    $c = New-Object Capture.W+RECT; [void][Capture.W]::GetClientRect($picker, [ref]$c)
    $x = [int]($c.Right - 41 * $Scale); $y = [int](30 * $Scale)
    $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
    [void][Capture.W]::PostMessage($picker, 0x0200, [IntPtr]::Zero, $lp)   # move
    [void][Capture.W]::PostMessage($picker, 0x0201, [IntPtr]1, $lp)        # down
    [void][Capture.W]::PostMessage($picker, 0x0202, [IntPtr]::Zero, $lp)   # up
    Start-Sleep -Milliseconds 900
}

$h = [Capture.W]::Find($id, $titles[$Window])
if ($h -eq [IntPtr]::Zero) { throw "The $Window window is not visible." }
Start-Sleep -Milliseconds 500   # let it finish appearing
$r = New-Object Capture.W+RECT
[void][Capture.W]::GetWindowRect($h, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap(($r.Right - $r.Left), ($r.Bottom - $r.Top))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc(); [void][Capture.W]::PrintWindow($h, $dc, 2); $g.ReleaseHdc($dc); $g.Dispose()

if ($ClientOnly) {
    $c = New-Object Capture.W+RECT; [void][Capture.W]::GetClientRect($h, [ref]$c)
    $o = New-Object Capture.W+POINT; [void][Capture.W]::ClientToScreen($h, [ref]$o)
    $crop = New-Object System.Drawing.Rectangle ($o.X - $r.Left), ($o.Y - $r.Top), $c.Right, $c.Bottom
} else {
    $v = New-Object Capture.W+RECT
    [void][Capture.W]::DwmGetWindowAttribute($h, 9, [ref]$v, 16)   # visible frame, without the invisible resize borders
    $crop = New-Object System.Drawing.Rectangle ($v.Left - $r.Left), ($v.Top - $r.Top), ($v.Right - $v.Left), ($v.Bottom - $v.Top)
}
if ($crop.Width -gt 0 -and $crop.Height -gt 0) { $cut = $bmp.Clone($crop, $bmp.PixelFormat); $bmp.Dispose(); $bmp = $cut }
$full = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
$bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
"Saved $($bmp.Width)x$($bmp.Height) to $full"
