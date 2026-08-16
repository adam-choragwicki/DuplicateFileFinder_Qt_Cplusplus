param(
    [switch]$Restore
)

$filePath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "root\unreadable.txt")).Path
$currentSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$principal = "*$currentSid"

if ($Restore) {
    & icacls.exe $filePath /remove:d $principal /Q | Out-Null

    if ($LASTEXITCODE -ne 0) {
        throw "Failed to restore read access to '$filePath'."
    }

    try {
        $stream = [System.IO.File]::OpenRead($filePath)
        $stream.Dispose()
    }
    catch {
        throw "The deny rule was removed, but the file is still unreadable: $($_.Exception.Message)"
    }

    Write-Host "Verified that read access was restored: $filePath"
    return
}

& icacls.exe $filePath /deny "${principal}:R" /Q | Out-Null

if ($LASTEXITCODE -ne 0) {
    throw "Failed to deny read access to '$filePath'."
}

$fileIsUnreadable = $false

try {
    $stream = [System.IO.File]::OpenRead($filePath)
    $stream.Dispose()
}
catch [System.UnauthorizedAccessException] {
    $fileIsUnreadable = $true
}
catch {
    & icacls.exe $filePath /remove:d $principal /Q | Out-Null
    throw "Could not verify the unreadable-file scenario; the deny rule was removed: $($_.Exception.Message)"
}

if (-not $fileIsUnreadable) {
    & icacls.exe $filePath /remove:d $principal /Q | Out-Null
    throw "The deny rule was applied, but the file is still readable. The rule has been removed."
}

Write-Host "Verified that the file is unreadable: $filePath"
