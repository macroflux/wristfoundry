param(
    [Parameter(Mandatory)] [ValidatePattern('^COM\d+$')] [string] $Port,
    [switch] $RecoveryReady,
    [switch] $AcknowledgePermanentHmacEfuseProvisioning
)

if (-not $RecoveryReady) {
    throw 'Refusing to flash without -RecoveryReady. Follow docs/RECOVERY.md first.'
}
if (-not $AcknowledgePermanentHmacEfuseProvisioning) {
    throw 'Refusing to flash: add -AcknowledgePermanentHmacEfuseProvisioning only after reading docs/SECURE_STORAGE.md.'
}

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

Write-Warning 'IRREVERSIBLE: first boot may permanently program the configured HMAC eFuse key block.'
Write-Warning 'A flash backup cannot undo eFuse programming.'
Write-Host 'Reading the current security state before flashing:'
Invoke-Checked -Command python -Arguments @(
    '-m', 'espefuse', '--chip', 'esp32s3', '--port', $Port, 'summary'
)

$buildDirectory = Join-Path $script:WristFoundryRoot 'build-secure'
if (-not (Test-Path -LiteralPath (Join-Path $buildDirectory 'wristfoundry.bin'))) {
    throw 'Secure firmware is not built. Run scripts/Build-Secure.ps1 first.'
}

Invoke-Checked -Command idf.py -Arguments @('-B', $buildDirectory, '-p', $Port, 'flash')
