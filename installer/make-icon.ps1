# Vygeneruje installer/Cubiq.ico: izometrická kocka trávy (16, 32, 48, 256 px, PNG vo vnútri ICO).
Add-Type -AssemblyName System.Drawing
$sizes = 16, 32, 48, 256
$pngs = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $c = $s / 2.0; $r = $s * 0.46; $k = 0.866 * $r
    $top = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF($c, ($c - $r))), (New-Object System.Drawing.PointF(($c + $k), ($c - $r / 2))),
        (New-Object System.Drawing.PointF($c, $c)), (New-Object System.Drawing.PointF(($c - $k), ($c - $r / 2))))
    $left = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF(($c - $k), ($c - $r / 2))), (New-Object System.Drawing.PointF($c, $c)),
        (New-Object System.Drawing.PointF($c, ($c + $r))), (New-Object System.Drawing.PointF(($c - $k), ($c + $r / 2))))
    $right = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF($c, $c)), (New-Object System.Drawing.PointF(($c + $k), ($c - $r / 2))),
        (New-Object System.Drawing.PointF(($c + $k), ($c + $r / 2))), (New-Object System.Drawing.PointF($c, ($c + $r))))
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 110, 190, 70))), $top)
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 120, 86, 58))), $left)
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 92, 64, 44))), $right)
    # pás trávy na bokoch
    $band = $r * 0.28
    $leftGrass = [System.Drawing.PointF[]]@($left[0], $left[1], (New-Object System.Drawing.PointF($c, ($c + $band))), (New-Object System.Drawing.PointF(($c - $k), ($c - $r / 2 + $band))))
    $rightGrass = [System.Drawing.PointF[]]@($right[0], $right[1], (New-Object System.Drawing.PointF(($c + $k), ($c - $r / 2 + $band))), (New-Object System.Drawing.PointF($c, ($c + $band))))
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 90, 160, 56))), $leftGrass)
    $g.FillPolygon((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 72, 136, 46))), $rightGrass)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += , $ms.ToArray()
}
$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter $out
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $dim = if ($s -ge 256) { 0 } else { $s }
    $w.Write([byte]$dim); $w.Write([byte]$dim); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32); $w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $w.Write($p) }
[System.IO.File]::WriteAllBytes((Join-Path $PSScriptRoot "Cubiq.ico"), $out.ToArray())
"Cubiq.ico hotova"
