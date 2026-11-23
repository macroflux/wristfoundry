# Submit the development candidate for CI; does not flash, merge, tag or release.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$repo = 'macroflux/wristfoundry'
$branch = 'feat/android-ble-companion'
gh auth status
if ($LASTEXITCODE -ne 0) { throw 'Run from your authenticated PowerShell.' }
$current = git branch --show-current
if ($LASTEXITCODE -ne 0 -or $current -ne $branch) { throw "Expected $branch" }
$changes = git status --porcelain
if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Resolve local changes before submitting.' }
git -c http.sslBackend=schannel push -u origin $branch
if ($LASTEXITCODE -ne 0) { throw 'Push failed' }
$existing = gh pr list --repo $repo --head $branch --state open --json url --jq '.[0].url'
if ($LASTEXITCODE -ne 0) { throw 'Could not read pull requests' }
if ($existing -and $existing.StartsWith('https://')) { Write-Host $existing; exit 0 }
$folder = Join-Path (Get-Location) 'build-phone-tools'
New-Item -ItemType Directory -Path $folder -Force > $null
$body = Join-Path $folder 'android-pr-body.md'
@'
Adds an Android Flutter companion over encrypted/authenticated BLE, using the
same controller, protocol, settings editor and app icons as Windows USB.
Watch/Windows/Android Settings now open section cards. The watch advertises for
60 seconds on request; an authenticated session holds the display awake and
expires after five minutes without user interaction. Polls do not extend it.
Disconnect releases the BLE stack and restores saved power behavior. PWR can
still blank the display. Radar and phone sessions share exclusive radio ownership.

Local validation: Flutter analysis, 28 tests, four Python contract/release tests,
native Kotlin compilation against Android headers, and ESP-Clang syntax checks.
Full firmware linking and APK/Windows packaging require CI. Local APK assembly
is blocked by Gradle network permissions; local ESP GCC cannot start (error 623).
Physical Android pairing, idle/disconnect/reconnect and battery acceptance are
pending. No candidate flashed or published. Version 0.3.0 remains unreleased.

See docs/ANDROID_COMPANION.md and docs/ANDROID_VALIDATION.md.
'@ | Set-Content -LiteralPath $body -Encoding utf8
gh pr create --repo $repo --base main --head $branch --draft --title 'Android BLE Companion and Settings sections' --body-file $body
if ($LASTEXITCODE -ne 0) { throw 'PR creation failed; the branch was pushed successfully.' }
