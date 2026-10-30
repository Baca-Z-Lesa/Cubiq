# Zostaví Release build a z neho installer dist\Cubiq-<verzia>-setup.exe (Inno Setup 6).
# Chyby kontrolujeme cez $LASTEXITCODE – varovania CMake na stderr nemajú build zastaviť.
$ErrorActionPreference = "Continue"
$root = Split-Path $PSScriptRoot -Parent

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) {
    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 nie je nainstalovany (https://jrsoftware.org/isinfo.php)" }

if (-not (Test-Path "$PSScriptRoot\Cubiq.ico")) { & "$PSScriptRoot\make-icon.ps1" }

& $cmake -S $root -B "$root\build"
if ($LASTEXITCODE) { throw "CMake configure zlyhal" }
& $cmake --build "$root\build" --config Release --target Cubiq -- /m /v:minimal
if ($LASTEXITCODE) { throw "Build zlyhal" }

& $iscc "$PSScriptRoot\Cubiq.iss"
if ($LASTEXITCODE) { throw "Inno Setup zlyhal" }
Get-ChildItem "$root\dist\*.exe" | ForEach-Object { "Hotovo: $($_.FullName)  ($([math]::Round($_.Length / 1MB, 1)) MB)" }
