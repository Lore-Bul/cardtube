# Generates the placeholder artwork used by the demo screenshots.
# The images are abstract gradients drawn here, so the store screenshots do not
# embed third-party thumbnails. Run before capturing CARDTUBE_DEMO screenshots.
param(
    [string]$ThumbDirectory = (Join-Path $env:TEMP "cardtube\thumbs")
)

Add-Type -AssemblyName System.Drawing

$width = 320
$height = 180

$specs = @(
    @{ File = "demo0000001.jpg"; Top = @(26, 18, 62); Bottom = @(228, 74, 148); Style = "sun" },
    @{ File = "demo0000002.jpg"; Top = @(10, 62, 74); Bottom = @(22, 150, 128); Style = "waves" },
    @{ File = "demo0000003.jpg"; Top = @(14, 30, 58); Bottom = @(66, 132, 214); Style = "grid" },
    @{ File = "demo0000004.jpg"; Top = @(58, 18, 52); Bottom = @(242, 132, 42); Style = "bars" },
    @{ File = "demo0000005.jpg"; Top = @(18, 32, 46); Bottom = @(96, 122, 148); Style = "rain" }
)

New-Item -ItemType Directory -Force -Path $ThumbDirectory | Out-Null

$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() |
    Where-Object { $_.MimeType -eq "image/jpeg" } | Select-Object -First 1
$encoderParams = New-Object System.Drawing.Imaging.EncoderParameters(1)
$encoderParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter(
    [System.Drawing.Imaging.Encoder]::Quality, 92L)

foreach ($spec in $specs) {
    $bmp = New-Object System.Drawing.Bitmap($width, $height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

    $top = [System.Drawing.Color]::FromArgb(255, $spec.Top[0], $spec.Top[1], $spec.Top[2])
    $bottom = [System.Drawing.Color]::FromArgb(255, $spec.Bottom[0], $spec.Bottom[1], $spec.Bottom[2])
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Point(0, 0)),
        (New-Object System.Drawing.Point(0, $height)),
        $top, $bottom)
    $g.FillRectangle($brush, 0, 0, $width, $height)
    $brush.Dispose()

    $accent = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(110, 255, 255, 255))
    $soft = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(60, 255, 255, 255))
    $dark = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90, 0, 0, 0))

    switch ($spec.Style) {
        "sun" {
            $g.FillEllipse($soft, 180, -40, 220, 220)
            $g.FillEllipse($accent, 214, 44, 96, 96)
            $g.FillRectangle($dark, 0, 148, $width, 32)
        }
        "waves" {
            for ($i = 0; $i -lt 6; $i++) {
                $y = 20 + $i * 26
                $rect = New-Object System.Drawing.Rectangle(0, $y, $width, 14)
                $g.FillPie($soft, $rect, 180, 180)
            }
            $g.FillEllipse($accent, 24, 58, 54, 54)
        }
        "grid" {
            $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(70, 255, 255, 255), 2)
            for ($i = 0; $i -le 8; $i++) { $g.DrawLine($pen, 0, $i * 22, $width, $i * 22) }
            for ($i = 0; $i -le 14; $i++) { $g.DrawLine($pen, $i * 24, 0, $i * 24, $height) }
            $pen.Dispose()
            $g.FillEllipse($dark, 116, 46, 88, 88)
            $g.FillEllipse($accent, 142, 72, 36, 36)
        }
        "bars" {
            $heights = @(34, 62, 96, 128, 84, 52, 110, 74, 40, 92, 58, 30)
            for ($i = 0; $i -lt $heights.Count; $i++) {
                $h = $heights[$i]
                $g.FillRectangle($accent, 16 + $i * 24, ($height - $h) / 2, 14, $h)
            }
        }
        "rain" {
            $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(80, 255, 255, 255), 2)
            for ($i = 0; $i -lt 34; $i++) {
                $x = ($i * 37) % $width
                $y = ($i * 61) % $height
                $g.DrawLine($pen, $x, $y, $x - 10, $y + 22)
            }
            $pen.Dispose()
            $g.FillEllipse($soft, 68, 44, 120, 120)
        }
    }

    $accent.Dispose()
    $soft.Dispose()
    $dark.Dispose()

    $path = Join-Path $ThumbDirectory $spec.File
    $bmp.Save($path, $codec, $encoderParams)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "thumb written: $path"
}
