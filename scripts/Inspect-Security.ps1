param(
    [Parameter(Mandatory)] [ValidatePattern('^COM\d+$')] [string] $Port
)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf

Write-Host 'Read-only ESP32-S3 eFuse summary:'
Invoke-Checked -Command python -Arguments @(
    '-m', 'espefuse', '--chip', 'esp32s3', '--port', $Port, 'summary'
)
