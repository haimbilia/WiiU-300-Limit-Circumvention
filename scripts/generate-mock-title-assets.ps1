[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$assets = Join-Path $root 'test-tools\mock-title-rpx\assets'
New-Item -ItemType Directory -Path $assets -Force | Out-Null

Add-Type -AssemblyName System.Drawing

function New-TestImage {
    param(
        [Parameter(Mandatory)] [string]$Path,
        [Parameter(Mandatory)] [int]$Width,
        [Parameter(Mandatory)] [int]$Height,
        [Parameter(Mandatory)] [float]$FontSize
    )

    $bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $background = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(27, 73, 125))
    $foreground = [Drawing.SolidBrush]::new([Drawing.Color]::White)
    $font = [Drawing.Font]::new('Arial', $FontSize, [Drawing.FontStyle]::Bold,
        [Drawing.GraphicsUnit]::Pixel)
    $format = [Drawing.StringFormat]::new()
    $format.Alignment = [Drawing.StringAlignment]::Center
    $format.LineAlignment = [Drawing.StringAlignment]::Center

    try {
        $graphics.FillRectangle($background, 0, 0, $Width, $Height)
        $graphics.DrawString("TITLE LIMIT`nTEST", $font, $foreground,
            [Drawing.RectangleF]::new(0, 0, $Width, $Height), $format)
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $format.Dispose()
        $font.Dispose()
        $foreground.Dispose()
        $background.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

New-TestImage -Path (Join-Path $assets 'icon.png') -Width 128 -Height 128 -FontSize 25
New-TestImage -Path (Join-Path $assets 'tv-splash.png') -Width 1280 -Height 720 -FontSize 110
New-TestImage -Path (Join-Path $assets 'drc-splash.png') -Width 854 -Height 480 -FontSize 75

Get-ChildItem -LiteralPath $assets -Filter '*.png' | ForEach-Object {
    [pscustomobject]@{
        File = $_.Name
        Bytes = $_.Length
        Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
    }
}
