param(
    [Parameter(Mandatory)] [ValidatePattern('^COM\d+$')] [string] $Port,
    [switch] $RecoveryReady
)

if (-not $RecoveryReady) {
    throw 'Refusing to flash without -RecoveryReady. First create and verify a factory backup as documented in docs/RECOVERY.md.'
}

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

Invoke-Checked -Command python -Arguments @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port, 'chip_id')
Invoke-Checked -Command idf.py -Arguments @('-p', $Port, 'flash')
