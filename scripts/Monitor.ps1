param([Parameter(Mandatory)] [ValidatePattern('^COM\d+$')] [string] $Port)

. (Join-Path $PSScriptRoot 'Common.ps1')
Enter-WristFoundryIdf
Invoke-Checked -Command idf.py -Arguments @('-p', $Port, 'monitor')
