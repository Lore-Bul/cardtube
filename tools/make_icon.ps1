# Generates the 256x256 store icon (assets/images/cardtube.png).
# Requires Windows PowerShell with System.Drawing (used during development only).
param(
    [string]$Output = (Join-Path $PSScriptRoot "..\assets\images\cardtube.png")
)

Add-Type -AssemblyName System.Drawing

$size = 256
$bmp = New-Object System.Drawing.Bitmap($size, $size)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

# Rounded background with a subtle vertical gradient.
$bgPath = New-RoundedPath 8 8 240 240 54
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(0, 8)),
    (New-Object System.Drawing.Point(0, 248)),
    [System.Drawing.Color]::FromArgb(255, 20, 24, 32),
    [System.Drawing.Color]::FromArgb(255, 9, 11, 16))
$g.FillPath($bgBrush, $bgPath)

# Teal accent ring.
$ringPath = New-RoundedPath 26 26 204 204 42
$ringPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 99, 226, 183), 10)
$g.DrawPath($ringPen, $ringPath)

# Screen highlight: darker panel behind the play glyph.
$panelPath = New-RoundedPath 52 52 152 152 28
$panelBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(52, 52)),
    (New-Object System.Drawing.Point(204, 204)),
    [System.Drawing.Color]::FromArgb(255, 30, 44, 54),
    [System.Drawing.Color]::FromArgb(255, 18, 26, 34))
$g.FillPath($panelBrush, $panelPath)

# Play triangle.
$play = New-Object System.Drawing.Drawing2D.GraphicsPath
$play.AddPolygon(@(
    (New-Object System.Drawing.Point(106, 86)),
    (New-Object System.Drawing.Point(178, 128)),
    (New-Object System.Drawing.Point(106, 170))))
$playBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.Point(106, 86)),
    (New-Object System.Drawing.Point(178, 170)),
    [System.Drawing.Color]::FromArgb(255, 180, 255, 226),
    [System.Drawing.Color]::FromArgb(255, 79, 208, 160))
$g.FillPath($playBrush, $play)

# Subtle inner glow line under the play glyph.
$glowPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(90, 99, 226, 183), 3)
$g.DrawLine($glowPen, 84, 186, 172, 186)

$g.Dispose()
$bmp.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Host "icon written: $Output"
