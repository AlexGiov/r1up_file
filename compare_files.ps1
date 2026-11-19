# PowerShell script to compare two files using SHA256 hash
# This forces complete file read, bypassing Windows filesystem cache issues
# Exit code: 0 = files identical, 1 = files differ

param(
    [Parameter(Mandatory = $true)]
    [string]$File1,
    
    [Parameter(Mandatory = $true)]
    [string]$File2
)

# Check if files exist
if (-not (Test-Path $File1)) {
    Write-Host "ERROR: File not found: $File1" -ForegroundColor Red
    exit 2
}

if (-not (Test-Path $File2)) {
    Write-Host "ERROR: File not found: $File2" -ForegroundColor Red
    exit 2
}

# Calculate SHA256 hashes
try {
    $hash1 = (Get-FileHash -Path $File1 -Algorithm SHA256).Hash
    $hash2 = (Get-FileHash -Path $File2 -Algorithm SHA256).Hash
    
    # Compare hashes
    if ($hash1 -eq $hash2) {
        Write-Host "Files are identical (SHA256: $hash1)" -ForegroundColor Green
        exit 0
    }
    else {
        Write-Host "Files differ:" -ForegroundColor Yellow
        Write-Host "  $File1" -ForegroundColor Yellow
        Write-Host "    SHA256: $hash1" -ForegroundColor Yellow
        Write-Host "  $File2" -ForegroundColor Yellow
        Write-Host "    SHA256: $hash2" -ForegroundColor Yellow
        exit 1
    }
}
catch {
    Write-Host "ERROR: Failed to compute hash: $_" -ForegroundColor Red
    exit 3
}
