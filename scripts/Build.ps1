param([switch] $Clean)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

if ($Clean -and (Test-Path -LiteralPath (Join-Path $script:WristFoundryRoot 'build'))) {
    Invoke-Checked idf.py fullclean
}

$sdkconfig = Join-Path $script:WristFoundryRoot 'sdkconfig'
$targetConfigured = (Test-Path -LiteralPath $sdkconfig) -and
    (Select-String -LiteralPath $sdkconfig -Pattern '^CONFIG_IDF_TARGET="esp32s3"$' -Quiet)

if (-not $targetConfigured) {
    Invoke-Checked idf.py set-target esp32s3
}

Invoke-Checked idf.py build
