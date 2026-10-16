param(
    [Parameter(Mandatory)]
    [ValidateSet('active', 'display-off', 'light-sleep', 'deep-sleep', 'wifi-active')]
    [string] $Mode,
    [switch] $Clean
)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

$modeValues = @{
    'active' = 1
    'display-off' = 2
    'light-sleep' = 3
    'deep-sleep' = 4
    'wifi-active' = 5
}
$buildDirectory = Join-Path $script:WristFoundryRoot "build-power-$Mode"

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Invoke-Checked idf.py -B $buildDirectory fullclean
}

Invoke-Checked idf.py -B $buildDirectory `
    "-DWRISTFOUNDRY_POWER_TEST_MODE=$($modeValues[$Mode])" build

Set-Content -LiteralPath (Join-Path $buildDirectory 'power-test-mode.txt') `
    -Value $Mode -NoNewline

Write-Host "Power-test image ready: $Mode"
Write-Host "Build directory: $buildDirectory"
