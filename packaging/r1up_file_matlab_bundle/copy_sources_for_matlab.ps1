param(
    [string]$DestinationRoot = "",
    [switch]$CleanDestination
)

$ErrorActionPreference = "Stop"

$bundleDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $bundleDir "../..")

if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $DestinationRoot = Join-Path $root "dist/r1up_file_matlab_src"
}

# Same C files used by packaging/r1up_sdk/build_static_mingw.bat.
$sourceMap = @(
    @{ Src = "src/mut/src/r1up_file.c"; Dst = "src/r1up_file.c" },
    @{ Src = "src/lib/ag_fsal/src/ag_fsal.c"; Dst = "src/ag_fsal.c" },
    @{ Src = "src/lib/ag_mal/src/ag_mal.c"; Dst = "src/ag_mal.c" },
    @{ Src = "src/lib/ag_fixedpoint/src/ag_fixedpoint.c"; Dst = "src/ag_fixedpoint.c" }
)

# Header layout mirrors the namespaced #include directives used by the sources:
# - #include "r1up_file/r1up_file.h"
# - #include "ag_fsal/ag_fsal.h"
# - #include "ag_mal/ag_mal.h"
# - #include "ag_fixedpoint/ag_fixedpoint.h"
# - #include "r1up_file_cfg.h", "ag_fsal_cfg.h", "ag_mal_cfg.h" (include root), "ag_fixedpoint_cfg.h" (included from ag_fixedpoint/)
$headerMap = @(
    @{ Src = "src/mut/include/r1up_file/r1up_file.h"; Dst = "include/r1up_file/r1up_file.h" },
    @{ Src = "src/mut/include/r1up_file/r1up_file_variable_type_database.h"; Dst = "include/r1up_file/r1up_file_variable_type_database.h" },
    @{ Src = "src/mut/include/r1up_file/r1up_file_rr_file_variable_type_block_database.h"; Dst = "include/r1up_file/r1up_file_rr_file_variable_type_block_database.h" },

    @{ Src = "src/lib/ag_fsal/include/ag_fsal/ag_fsal.h"; Dst = "include/ag_fsal/ag_fsal.h" },
    @{ Src = "src/lib/ag_mal/include/ag_mal/ag_mal.h"; Dst = "include/ag_mal/ag_mal.h" },
    @{ Src = "src/lib/ag_fixedpoint/include/ag_fixedpoint/ag_fixedpoint.h"; Dst = "include/ag_fixedpoint/ag_fixedpoint.h" },

    @{ Src = "src/cfg/r1up_file_cfg.h"; Dst = "include/r1up_file_cfg.h" },
    @{ Src = "src/cfg/ag_fsal_cfg.h"; Dst = "include/ag_fsal_cfg.h" },
    @{ Src = "src/cfg/ag_mal_cfg.h"; Dst = "include/ag_mal_cfg.h" },
    @{ Src = "src/cfg/ag_fixedpoint_cfg.h"; Dst = "include/ag_fixedpoint/ag_fixedpoint_cfg.h" }
)

$dest = $DestinationRoot
$destInclude = Join-Path $dest "include"
$destSrc = Join-Path $dest "src"

Write-Host "Preparing MATLAB source bundle (SDK-like include/src layout)..."
Write-Host "Source root:      $root"
Write-Host "Destination root: $dest"

if ($CleanDestination -and (Test-Path $dest)) {
    Write-Host "Cleaning destination..."
    # Contents are removed instead of the folder itself, which may be in use (open in a shell/Explorer or shared).
    Get-ChildItem $dest -Force | Remove-Item -Recurse -Force
}

New-Item -ItemType Directory -Path $destInclude -Force | Out-Null
New-Item -ItemType Directory -Path $destSrc -Force | Out-Null

foreach ($m in $sourceMap) {
    $src = Join-Path $root $m.Src
    if (-not (Test-Path $src)) {
        throw "Missing source file: $src"
    }

    $dst = Join-Path $dest $m.Dst
    $dstDir = Split-Path -Parent $dst
    New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    Copy-Item -Path $src -Destination $dst -Force
    Write-Host "Copied source: $($m.Src) -> $($m.Dst)"
}

foreach ($m in $headerMap) {
    $src = Join-Path $root $m.Src
    if (-not (Test-Path $src)) {
        throw "Missing header file: $src"
    }

    $dst = Join-Path $dest $m.Dst
    $dstDir = Split-Path -Parent $dst
    New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    Copy-Item -Path $src -Destination $dst -Force
}

Write-Host "Copied headers for include layout."

# Force MATLAB build switch in destination configuration.
$destCfg = Join-Path $dest "include/r1up_file_cfg.h"
if (-not (Test-Path $destCfg)) {
    throw "Destination config header not found: $destCfg"
}

$cfgContent = Get-Content -Raw -Path $destCfg
$cfgUpdated = $cfgContent -replace '#define\s+R1UP_FILE_BUILD_ON_MATLAB\s+0', '#define R1UP_FILE_BUILD_ON_MATLAB 1'
if ($cfgUpdated -eq $cfgContent) {
    Write-Warning "R1UP_FILE_BUILD_ON_MATLAB define not patched (pattern not found or already set)."
}
Set-Content -Path $destCfg -Value $cfgUpdated -Encoding ASCII
Write-Host "Patched MATLAB flag in: include/r1up_file_cfg.h"

$manifest = @()
$manifest += "MATLAB source bundle generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$manifest += "Source root: $root"
$manifest += "Destination root: $dest"
$manifest += ""
$manifest += "Layout:"
$manifest += "- include/ (SDK-like header layout)"
$manifest += "- src/     (C sources compiled for static library)"
$manifest += ""
$manifest += "C sources:"
$sourceMap | ForEach-Object { $manifest += "- $($_.Dst)" }
$manifest += ""
$manifest += "Headers:"
$headerMap | ForEach-Object { $manifest += "- $($_.Dst)" }

$manifestPath = Join-Path $dest "BUNDLE_MANIFEST.txt"
Set-Content -Path $manifestPath -Value $manifest -Encoding ASCII

$matlabHint = @()
$matlabHint += "% Example MEX compile (adjust as needed). Run from the bundle folder; the MEX entry point is mexFunction in r1up_file.c:"
$matlabHint += "% mex -Iinclude src/r1up_file.c src/ag_fsal.c src/ag_mal.c src/ag_fixedpoint.c"
$matlabHint += "%"
$matlabHint += "% For a static lib workflow inside MATLAB toolchain, reuse the same sources and -Iinclude."
$matlabHintPath = Join-Path $dest "MATLAB_BUILD_HINT.txt"
Set-Content -Path $matlabHintPath -Value $matlabHint -Encoding ASCII

Write-Host "Bundle completed successfully."
Write-Host "Manifest: $manifestPath"
Write-Host "Hint:     $matlabHintPath"
