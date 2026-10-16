param(
    [Parameter(Mandatory)]
    [ValidateSet('active', 'display-off', 'light-sleep', 'deep-sleep', 'wifi-active')]
    [string] $Mode,
    [Parameter(Mandatory)] [ValidatePattern('^COM\d+$')] [string] $Port,
    [switch] $RecoveryReady
)

if (-not $RecoveryReady) {
    throw 'Refusing to flash without -RecoveryReady. Preserve the factory backup as documented in docs/RECOVERY.md.'
}

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

$buildDirectory = Join-Path $script:WristFoundryRoot "build-power-$Mode"
if (-not (Test-Path -LiteralPath (Join-Path $buildDirectory 'wristfoundry.bin'))) {
    throw "No built $Mode image found. Run .\scripts\Build-PowerTest.ps1 -Mode $Mode first."
}
$modeMarker = Join-Path $buildDirectory 'power-test-mode.txt'
if (-not (Test-Path -LiteralPath $modeMarker) -or
    (Get-Content -LiteralPath $modeMarker -Raw).Trim() -ne $Mode) {
    throw "The current power-test image is not '$Mode'. Rebuild that mode before flashing."
}

Invoke-Checked -Command python -Arguments @('-m', 'esptool', '--chip', 'esp32s3', '--port', $Port, 'chip_id')
Invoke-Checked -Command idf.py -Arguments @('-B', $buildDirectory, '-p', $Port, 'flash')
