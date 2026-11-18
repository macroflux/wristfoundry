param([switch] $Clean)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

$buildDirectory = Join-Path $script:WristFoundryRoot 'build-secure'
if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    Invoke-Checked idf.py -B $buildDirectory fullclean
}

Write-Warning 'IRREVERSIBLE FIRST-BOOT BEHAVIOR: this profile can permanently program the selected HMAC eFuse key block.'
Write-Warning 'Building is safe; flashing and booting the result is the irreversible boundary.'
Write-Warning 'Read docs/SECURE_STORAGE.md and inspect the target device before flashing.'
Invoke-Checked idf.py -B $buildDirectory `
    '-DSDKCONFIG=build-secure/sdkconfig' `
    '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.secure.defaults' build
