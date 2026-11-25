# Builds build\KidsMode-Setup.exe: deploys KidsMode.exe with the Qt runtime and the
# MSVC CRT, then packs everything into a single offline installer with the Qt
# Installer Framework. Build the Release target first.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$qt   = "E:\Qt\6.11.0\msvc2022_64\bin"
$ifw  = "E:\Qt\Tools\QtInstallerFramework\4.10\bin"
$build = Join-Path $root "build\Desktop_Qt_6_11_0_MSVC2022_64bit_Release"
$stage = Join-Path $root "build\installer-stage"
$data  = Join-Path $PSScriptRoot "packages\com.kidsmode.app\data"

if (-not (Test-Path (Join-Path $build "KidsMode.exe"))) { throw "build KidsMode Release first" }

# 1. Stage the app: exe + Qt DLLs/plugins + VC runtime (not on every PC).
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $build "KidsMode.exe") $stage
& (Join-Path $qt "windeployqt.exe") --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage "KidsMode.exe") | Out-Null
$crt = Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT\*.dll" -ErrorAction SilentlyContinue |
    Group-Object DirectoryName | Sort-Object Name | Select-Object -Last 1
if ($crt) { Copy-Item $crt.Group.FullName $stage } else { Write-Warning "VC redist not found; installer will need the VC++ runtime installed" }

# 2. Pack: mirror the stage into the package payload, then run binarycreator.
robocopy $stage $data /MIR /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) { throw "staging copy failed" }
$setup = Join-Path $root "build\KidsMode-Setup.exe"
Remove-Item $setup -Force -ErrorAction SilentlyContinue
& (Join-Path $ifw "binarycreator.exe") --offline-only -t (Join-Path $ifw "installerbase.exe") `
    -c (Join-Path $PSScriptRoot "config.xml") -p (Join-Path $PSScriptRoot "packages") $setup
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $setup)) { throw "binarycreator failed" }
"created $setup ({0:N1} MB)" -f ((Get-Item $setup).Length / 1MB)
