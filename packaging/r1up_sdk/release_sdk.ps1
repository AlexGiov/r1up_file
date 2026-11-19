param(
    [string]$Version = "1.0.0",
    [switch]$SkipSmokeTest
)

$ErrorActionPreference = "Stop"

$sdkDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $sdkDir "../..")
$buildScript = Join-Path $sdkDir "build_static_mingw.bat"
$stageScript = Join-Path $sdkDir "stage_sdk.ps1"
$templateDir = Join-Path $sdkDir "example_template"

$pkgDir = Join-Path $root "dist/r1up-sdk"
$libPath = Join-Path $root "build/r1up_sdk/lib/r1up_file.lib"
$zipPath = Join-Path $root ("dist/r1up-sdk-" + $Version + ".zip")

Write-Host "[1/5] Building static library..."
Push-Location $root
try {
    & $buildScript
    if ($LASTEXITCODE -ne 0) {
        throw "Static library build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

Write-Host "[2/5] Staging SDK files..."
& powershell -ExecutionPolicy Bypass -File $stageScript -Version $Version -LibSourcePath $libPath
if ($LASTEXITCODE -ne 0) {
    throw "SDK staging failed with exit code $LASTEXITCODE"
}

Write-Host "[3/5] Copying example template into $pkgDir ..."
Copy-Item -Path (Join-Path $templateDir "*") -Destination $pkgDir -Recurse -Force

# The template build.bat is written for a fixed SDK folder name; adapt only the copy in dist.
$buildBat = Join-Path $pkgDir "build.bat"
(Get-Content $buildBat -Raw).Replace("r1up-sdk-1.0.0", "r1up-sdk-$Version") | Set-Content $buildBat -NoNewline

if (-not $SkipSmokeTest) {
    Write-Host "[4/5] Running smoke test in $pkgDir ..."
    $savedPath = $env:PATH
    if (-not (Get-Command gcc.exe -ErrorAction SilentlyContinue)) {
        $devbin = if ($env:DEVBIN) { $env:DEVBIN } else { "C:\devbin" }
        $env:PATH = (Join-Path $devbin "mingw\mingw64\8.1.0\bin") + ";" + $env:PATH
    }
    Push-Location $pkgDir
    try {
        & cmd.exe /c "build.bat"
        if ($LASTEXITCODE -ne 0) {
            throw "Smoke test failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
        $env:PATH = $savedPath
        Get-ChildItem $pkgDir -Filter *.exe -File | Remove-Item -Force
    }
}
else {
    Write-Host "[4/5] Smoke test skipped by request."
}

Write-Host "[5/5] Creating zip package..."
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path $pkgDir -DestinationPath $zipPath -Force

Write-Host "Release completed successfully."
Write-Host "Package folder: $pkgDir"
Write-Host "Package zip:    $zipPath"
