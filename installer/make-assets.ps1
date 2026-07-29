<#
  Regenerates the SlowTime installer branding assets from the source logos in
  Resources/Logo/, using ImageMagick (crisp multi-size icon + sharp wizard art):

    assets/slowtime.ico     - installer .exe icon (SlowTime logo, 16..256 multi-size)
    assets/wizard-large.bmp - 164x314 left banner (SlowTime logo on white)
    assets/wizard-small.bmp - 55x58  top-right (LowHigh Sounds mark on white)

  These assets are committed to the repo, so CI does NOT need ImageMagick.
  Run this only when the source logos change:
    powershell -ExecutionPolicy Bypass -File installer/make-assets.ps1
#>

$ErrorActionPreference = 'Stop'
$logoDir   = Join-Path (Split-Path -Parent $PSScriptRoot) 'Resources\Logo'
$assetsDir = Join-Path $PSScriptRoot 'assets'
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null

$magick = Get-ChildItem "C:\Program Files\ImageMagick*\magick.exe" -ErrorAction SilentlyContinue |
          Select-Object -First 1 -ExpandProperty FullName
if (-not $magick) { throw "ImageMagick not found. Install it: winget install ImageMagick.ImageMagick" }

$slow = Join-Path $logoDir 'slowtime_logo.png'
$mark = Join-Path $logoDir 'lowhigh-mark.png'

Write-Host "Generating installer assets with $magick ..."

# .exe icon: trim, fit centred in a 512 square, emit crisp per-size images.
& $magick $slow -trim +repage -filter Lanczos -resize 470x470 `
    -background none -gravity center -extent 512x512 `
    -define icon:auto-resize=256,128,96,64,48,32,16 (Join-Path $assetsDir 'slowtime.ico')
Write-Host "  wrote slowtime.ico (16..256)"

# Wizard left banner (164x314): SlowTime logo on white.
& $magick $slow -trim +repage -filter Lanczos -resize 150x292 `
    -background white -gravity center -extent 164x314 -flatten (Join-Path $assetsDir 'wizard-large.bmp' | ForEach-Object { "BMP3:$_" })
Write-Host "  wrote wizard-large.bmp (164x314)"

# Wizard top-right (55x58): LowHigh Sounds mark on white.
& $magick $mark -trim +repage -filter Lanczos -resize 49x52 `
    -background white -gravity center -extent 55x58 -flatten (Join-Path $assetsDir 'wizard-small.bmp' | ForEach-Object { "BMP3:$_" })
Write-Host "  wrote wizard-small.bmp (55x58)"

Write-Host "Done."
