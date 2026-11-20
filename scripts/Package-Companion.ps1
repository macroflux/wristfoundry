Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$desktop = Join-Path $PSScriptRoot '../companion/desktop'
$release = Join-Path $desktop 'build/windows/x64/runner/Release'
foreach ($required in @('wristfoundry_companion.exe', 'flutter_windows.dll', 'data/icudtl.dat', 'data/flutter_assets')) {
    if (-not (Test-Path -LiteralPath (Join-Path $release $required))) {
        throw "Incomplete Flutter runtime bundle: $required is missing. Run flutter build windows --release first."
    }
}
$packages = Join-Path $desktop 'build/packages'
New-Item -ItemType Directory -Path $packages -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $desktop 'THIRD_PARTY_FLUTTER_LICENSE') -Destination $release
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../LICENSE') -Destination $release
$zip = Join-Path $packages 'wristfoundry-companion-windows-x64.zip'
Compress-Archive -Path (Join-Path $release '*') -DestinationPath $zip -Force
$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
"$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($zip))" |
    Set-Content -LiteralPath (Join-Path $packages 'SHA256SUMS') -Encoding utf8
Write-Host 'Packaged the full Windows runtime directory. No release was published.'
