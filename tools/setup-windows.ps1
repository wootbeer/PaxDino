# One-shot Windows setup: from a fresh clone to a built game, ready to open in
# Visual Studio.
#
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#   .\tools\setup-windows.ps1 -Rom "C:\path\to\Turok - Rage Wars (USA).n64"
#
# It runs the steps in BUILDING.md:
#   1. sets up Visual Studio's x64 build environment (and puts its clang-cl on
#      PATH, as the CMake presets expect);
#   2. fetches the submodules if they're missing;
#   3. converts the ROM to big-endian turokrw.us.z64 in the repo root (any of
#      .z64/.v64/.n64 byte orders) and checks its SHA-1;
#   4. configures with the "windows-release" preset (CMakePresets.json), builds
#      N64Recomp and runs it to generate RecompiledFuncs/;
#   5. builds PaxDino (skip with -SkipGameBuild to build it
#      from Visual Studio instead).
#
# -Rom can be left out once turokrw.us.z64 is in place. Rerun with -Regenerate
# after changing turokrw.us.toml or the symbol file.
param(
    [string]$Rom,
    [string]$Preset = "windows-release",
    [switch]$Regenerate,
    [switch]$SkipGameBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$z64Name = "turokrw.us.z64"
$z64Sha1 = "9F8CB190831945F6F707AA8C2B20E04FA4276795"
$buildDir = switch ($Preset) {
    "windows-release"        { "build" }
    "windows-relwithdebinfo" { "build-relwithdebinfo" }
    "windows-debug"          { "build-debug" }
    default { throw "Unknown preset $Preset (windows-release, windows-relwithdebinfo, windows-debug)." }
}

# 1. Visual Studio's x64 build environment (compiler libraries, Windows SDK,
# Ninja, CMake) plus the bundled clang-cl.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio not found (no vswhere.exe) -- see BUILDING.md." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No Visual Studio install with the C++ tools found -- see BUILDING.md." }
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") {
    & (Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    Set-Location $root
}
$llvmBin = Join-Path $vsPath "VC\Tools\Llvm\x64\bin"
if (-not (Test-Path (Join-Path $llvmBin "clang-cl.exe"))) {
    throw "clang-cl not found in $llvmBin -- install `"C++ Clang Compiler for Windows`" in the Visual Studio Installer."
}
$env:PATH = "$llvmBin;$env:PATH"
foreach ($tool in @("cmake", "ninja", "git")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found -- install `"C++ CMake tools for Windows`" in the Visual Studio Installer (and Git for Windows)."
    }
}

# 2. Submodules.
if (-not (Test-Path "lib\N64ModernRuntime\CMakeLists.txt") -or
    -not (Test-Path "lib\rt64\CMakeLists.txt") -or
    -not (Test-Path "lib\RecompFrontend\CMakeLists.txt")) {
    Write-Host "Fetching submodules..."
    git submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed." }
    foreach ($submodule in @("N64ModernRuntime", "rt64", "RecompFrontend")) {
        if (-not (Test-Path "lib\$submodule\CMakeLists.txt")) {
            throw "lib\$submodule is still empty after git submodule update."
        }
    }
}

# 3. The ROM, as big-endian .z64.
Add-Type -TypeDefinition @"
public static class TrwRomSwap {
    // v64: 16-bit byte swap.
    public static void Swap16(byte[] b) {
        for (int i = 0; i + 1 < b.Length; i += 2) { byte t = b[i]; b[i] = b[i + 1]; b[i + 1] = t; }
    }
    // n64: 32-bit little-endian words.
    public static void Swap32(byte[] b) {
        for (int i = 0; i + 3 < b.Length; i += 4) {
            byte t0 = b[i], t1 = b[i + 1];
            b[i] = b[i + 3]; b[i + 1] = b[i + 2]; b[i + 2] = t1; b[i + 3] = t0;
        }
    }
}
"@
$z64Path = Join-Path $root $z64Name
if ($Rom) {
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $Rom))
    $magic = "{0:X2}{1:X2}{2:X2}{3:X2}" -f $bytes[0], $bytes[1], $bytes[2], $bytes[3]
    switch ($magic) {
        "80371240" { }
        "37804012" { [TrwRomSwap]::Swap16($bytes) }
        "40123780" { [TrwRomSwap]::Swap32($bytes) }
        default { throw "$Rom doesn't look like an N64 ROM (header $magic)." }
    }
    [System.IO.File]::WriteAllBytes($z64Path, $bytes)
    Write-Host "Wrote $z64Name"
}
if (-not (Test-Path $z64Path)) {
    throw "$z64Name not found in the repo root -- pass -Rom <path to your dump>."
}
$hash = (Get-FileHash $z64Path -Algorithm SHA1).Hash
if ($hash -ne $z64Sha1) {
    throw "$z64Name has SHA-1 $hash, expected $z64Sha1 (Turok: Rage Wars, USA). See docs/ROM_LAYOUT.md."
}

# 4. Configure, build N64Recomp, generate RecompiledFuncs/. Configuring first
# applies lib-patches/, including the N64Recomp fix this game needs.
cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

if ($Regenerate -or -not (Test-Path "RecompiledFuncs\recomp_overlays.inl")) {
    cmake --build $buildDir --target N64RecompCLI
    if ($LASTEXITCODE -ne 0) { throw "Building N64Recomp failed." }
    $n64recomp = Get-ChildItem $buildDir -Recurse -Filter "N64Recomp.exe" | Select-Object -First 1
    if (-not $n64recomp) { throw "N64Recomp.exe not found under $buildDir." }
    Write-Host "Running N64Recomp..."
    & $n64recomp.FullName "turokrw.us.toml"
    if ($LASTEXITCODE -ne 0) { throw "N64Recomp failed." }
    # Pick up the new RecompiledFuncs/*.c.
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
}

# 5. The game.
if (-not $SkipGameBuild) {
    cmake --build $buildDir --target PaxDino
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
    Write-Host ""
    Write-Host "Built $buildDir\PaxDino.exe"
}
Write-Host "To work in Visual Studio: File > Open > Folder... on $root, pick the"
Write-Host "`"Windows x64 Release`" configuration and PaxDino.exe as the startup item."
