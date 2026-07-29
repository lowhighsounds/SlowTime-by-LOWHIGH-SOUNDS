<#
  Generates the SlowTime installer branding assets from the source logos in
  Resources/Logo/, using only .NET System.Drawing (no ImageMagick needed):

    assets/slowtime.ico     - installer .exe icon (SlowTime logo, multi-size)
    assets/wizard-large.bmp - 164x314 left banner (SlowTime logo on white)
    assets/wizard-small.bmp - 55x58  top-right corner (LowHigh Sounds mark)

  Run from anywhere: powershell -ExecutionPolicy Bypass -File installer/make-assets.ps1
#>

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'Stop'
$root      = Split-Path -Parent $PSScriptRoot   # project root
$logoDir   = Join-Path $root 'Resources\Logo'
$assetsDir = Join-Path $PSScriptRoot 'assets'
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null

function Load-Png([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $ms = New-Object System.IO.MemoryStream(,$bytes)
    return [System.Drawing.Bitmap]::FromStream($ms)
}

# Crop fully-transparent margins so the artwork fills its target box.
function Trim-Transparent([System.Drawing.Bitmap]$bmp) {
    $w = $bmp.Width; $h = $bmp.Height
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                          [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $data.Stride
    $buf = New-Object byte[] ($stride * $h)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
    $bmp.UnlockBits($data)

    $minX = $w; $minY = $h; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $h; $y++) {
        $row = $y * $stride
        for ($x = 0; $x -lt $w; $x++) {
            if ($buf[$row + $x * 4 + 3] -gt 8) {
                if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt $minX) { return $bmp.Clone() }
    $crop = New-Object System.Drawing.Rectangle $minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1)
    return $bmp.Clone($crop, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function New-Graphics([System.Drawing.Bitmap]$bmp) {
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    return $g
}

# Destination rect that fits src into (w,h) preserving aspect, centred, with margin.
function Fit-Rect($srcW, $srcH, $w, $h, $marginFrac) {
    $availW = $w * (1 - 2 * $marginFrac)
    $availH = $h * (1 - 2 * $marginFrac)
    $scale = [Math]::Min($availW / $srcW, $availH / $srcH)
    $dw = $srcW * $scale; $dh = $srcH * $scale
    return New-Object System.Drawing.RectangleF (($w - $dw) / 2), (($h - $dh) / 2), $dw, $dh
}

function Make-WizardBmp([System.Drawing.Bitmap]$art, $w, $h, $margin, $outPath) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = New-Graphics $bmp
    $g.Clear([System.Drawing.Color]::White)
    $r = Fit-Rect $art.Width $art.Height $w $h $margin
    $g.DrawImage($art, $r)
    $g.Dispose()
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $bmp.Dispose()
    Write-Host "  wrote $outPath ($w x $h)"
}

# Let GDI+ build the icon so it's a guaranteed-valid Windows .ico (a hand-rolled
# DIB is fragile). GetHicon produces a proper 256x256 icon that Windows scales
# down for smaller displays -- plenty for an installer icon.
function Make-Ico([System.Drawing.Bitmap]$art, $s, $outPath) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = New-Graphics $bmp
    $g.Clear([System.Drawing.Color]::Transparent)
    $r = Fit-Rect $art.Width $art.Height $s $s 0.06
    $g.DrawImage($art, $r)
    $g.Dispose()

    $hicon = $bmp.GetHicon()
    $icon = [System.Drawing.Icon]::FromHandle($hicon)
    $fs = [System.IO.File]::Open($outPath, [System.IO.FileMode]::Create)
    $icon.Save($fs)
    $fs.Close(); $icon.Dispose(); $bmp.Dispose()
    Write-Host "  wrote $outPath ($s x $s)"
}

Write-Host "Generating installer assets..."

$slowRaw = Load-Png (Join-Path $logoDir 'slowtime_logo.png')
$markRaw = Load-Png (Join-Path $logoDir 'lowhigh-mark.png')
$slow = Trim-Transparent $slowRaw
$mark = Trim-Transparent $markRaw

Make-WizardBmp $slow 164 314 0.10 (Join-Path $assetsDir 'wizard-large.bmp')
Make-WizardBmp $mark 55  58  0.10 (Join-Path $assetsDir 'wizard-small.bmp')
Make-Ico       $slow 256 (Join-Path $assetsDir 'slowtime.ico')

$slow.Dispose(); $mark.Dispose(); $slowRaw.Dispose(); $markRaw.Dispose()
Write-Host "Done."
