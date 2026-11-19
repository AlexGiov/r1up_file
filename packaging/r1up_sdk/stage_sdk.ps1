param(
    [string]$Version = "1.0.0",
    [string]$LibSourcePath = "",
    # Header source path (relative to src/test/unit) -> sub-folder of the SDK include dir ("" = include root).
    # The layout matches the #include directives used by the public headers.
    [hashtable]$HeaderMap = @{
        "src/mut/include/r1up_file/r1up_file.h"                           = "r1up_file"
        "src/mut/include/r1up_file/r1up_file_variable_type_database.h"    = "r1up_file"
        "src/cfg/r1up_file_cfg.h"                                         = ""
        "src/cfg/ag_fsal_cfg.h"                                           = ""
        "src/lib/ag_fixedpoint/include/ag_fixedpoint/ag_fixedpoint.h"     = "ag_fixedpoint"
        "src/cfg/ag_fixedpoint_cfg.h"                                     = "ag_fixedpoint"
    }
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "../..")
$distRoot = Join-Path (Join-Path $root "dist") "r1up-sdk"
$sdkRoot = Join-Path $distRoot ("r1up-sdk-" + $Version)
$includeRoot = Join-Path $sdkRoot "include"
$libRoot = Join-Path $sdkRoot "lib"

Write-Host "Staging SDK at: $sdkRoot"

# The whole package folder is emptied, so it only ever contains one SDK version.
# Contents are removed instead of the folder itself, which may be in use (e.g. open in a shell or Explorer).
if (Test-Path $distRoot) {
    Get-ChildItem $distRoot -Force | Remove-Item -Recurse -Force
}

New-Item -ItemType Directory -Path $includeRoot -Force | Out-Null
New-Item -ItemType Directory -Path $libRoot | Out-Null

foreach ($relPath in ($HeaderMap.Keys | Sort-Object)) {
    $src = Join-Path $root $relPath
    if (-not (Test-Path $src)) {
        throw "Header not found: $src"
    }

    $destDir = Join-Path $includeRoot $HeaderMap[$relPath]
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir | Out-Null
    }
    Copy-Item $src $destDir
}

if ([string]::IsNullOrWhiteSpace($LibSourcePath)) {
    Write-Warning "LibSourcePath not provided. Header staging completed, but no static library copied."
}
else {
    $libSrc = Resolve-Path $LibSourcePath

    # Stage the archive also under the .lib name (it is a GNU ar archive built by MinGW, not an MSVC import/static lib).
    Copy-Item $libSrc (Join-Path $libRoot "r1up_file.lib")
    Write-Host "Copied static library (.lib) from: $libSrc"

    # If a GNU archive exists next to the provided .lib, stage it too for MinGW/Linux-style consumers.
    $libDir = Split-Path $libSrc -Parent
    $gnuArchiveSrc = Join-Path $libDir "libr1up_file.a"
    if (Test-Path $gnuArchiveSrc) {
        Copy-Item $gnuArchiveSrc (Join-Path $libRoot "libr1up_file.a")
        Write-Host "Copied static library (.a) from: $gnuArchiveSrc"
    }
    else {
        Write-Warning "GNU archive libr1up_file.a not found next to LibSourcePath; only .lib staged."
    }
}

Write-Host "SDK staging completed."
Write-Host "Include folder: $includeRoot"
Write-Host "Lib folder: $libRoot"
