<#
  Runs CMake for native-desktop inside the MSVC x64 environment.

    powershell -File native-desktop/tools/native.ps1 configure windows-dev
    powershell -File native-desktop/tools/native.ps1 build     windows-dev
    powershell -File native-desktop/tools/native.ps1 test      windows-dev
    powershell -File native-desktop/tools/native.ps1 run       windows-dev
    powershell -File native-desktop/tools/native.ps1 install   windows-release <staging-directory>

  Toolchain (see docs/en/Features/Native Desktop Client.md):
    KATSODI_TOOLCHAIN  folder with venv/ (CMake, Ninja) and Qt/6.11.3/msvc2022_64,
                       default: the KatsodiToolchain folder next to the repository.
    Visual Studio 2022 Build Tools with the C++ workload (found with vswhere).
#>
param(
  [Parameter(Mandatory = $true)][ValidateSet("configure", "build", "test", "run", "install")][string]$Action,
  [ValidateSet("windows-dev", "windows-release")][string]$Preset = "windows-dev",
  [string]$Prefix = ""
)
$ErrorActionPreference = "Stop"
$source = Split-Path -Parent $PSScriptRoot
$toolchain = if ($env:KATSODI_TOOLCHAIN) { $env:KATSODI_TOOLCHAIN } else { Join-Path (Split-Path -Parent (Split-Path -Parent $source)) "KatsodiToolchain" }
$qt = Join-Path $toolchain "Qt\6.11.3\msvc2022_64"
$tools = Join-Path $toolchain "venv\Scripts"
if (-not (Test-Path (Join-Path $qt "lib\cmake\Qt6"))) { throw "Qt 6.11.3 (MSVC) not found in $qt. Run: python native-desktop/tools/install-qt.py `"$toolchain`"" }
if (-not (Test-Path (Join-Path $tools "cmake.exe"))) { throw "CMake/Ninja not found in $tools (pip install cmake ninja into that venv)." }

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "Visual Studio 2022 Build Tools with the C++ workload are required." }
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
# Import the MSVC environment into this PowerShell process.
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
  if ($_ -match "^([^=]+)=(.*)$") { [Environment]::SetEnvironmentVariable($matches[1], $matches[2]) }
}
$env:PATH = "$tools;$qt\bin;$env:PATH"
$env:KATSODI_QT_DIR = $qt
# Optional voice SDK (python native-desktop/tools/install-livekit.py <toolchain>).
$livekit = Join-Path $toolchain "livekit/1.13.1/livekit-sdk-windows-x64-1.13.1"
if (Test-Path (Join-Path $livekit "lib/cmake/LiveKit")) { $env:KATSODI_LIVEKIT_DIR = $livekit; $env:PATH = "$livekit/bin;$env:PATH" }
else { Write-Warning "LiveKit SDK not found: building without voice." }
# Optional WebView2 SDK for the provider player (python native-desktop/tools/install-webview2.py <toolchain>).
$webview2 = Join-Path $toolchain "webview2/1.0.4314.35"
if (Test-Path (Join-Path $webview2 "build/native/include/WebView2.h")) { $env:KATSODI_WEBVIEW2_DIR = $webview2 }
else { Write-Warning "WebView2 SDK not found: building without the provider player." }

Push-Location $source
try {
  switch ($Action) {
    "configure" { cmake --preset $Preset; if ($LASTEXITCODE) { exit $LASTEXITCODE } }
    "build" {
      if (-not (Test-Path "build\$Preset\CMakeCache.txt")) { cmake --preset $Preset; if ($LASTEXITCODE) { exit $LASTEXITCODE } }
      cmake --build --preset $Preset; if ($LASTEXITCODE) { exit $LASTEXITCODE }
    }
    "test" { ctest --preset $Preset; if ($LASTEXITCODE) { exit $LASTEXITCODE } }
    "run" { & "build\$Preset\Katsodi.exe" }
    "install" {
      if (-not $Prefix) { throw "install needs a staging directory" }
      cmake --install "build\$Preset" --prefix $Prefix; if ($LASTEXITCODE) { exit $LASTEXITCODE }
    }
  }
} finally {
  Pop-Location
}
