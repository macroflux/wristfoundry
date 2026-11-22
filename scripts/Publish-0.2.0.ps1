# Run in the owner's authenticated PowerShell. Releases the approved 0.2.0 update.
# No hardware operations, NVS changes, or security provisioning.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
$repo = 'macroflux/wristfoundry'
$tag = 'v0.2.0'
Invoke-Checked gh @('auth', 'status')
$branch = git branch --show-current
if ($LASTEXITCODE -ne 0 -or $branch -ne 'feat/persistent-settings-icons') { throw 'Expected feature branch' }
$changes = git status --porcelain
if ($LASTEXITCODE -ne 0 -or $changes) { throw 'Commit or resolve local changes before release' }
$head = (git rev-parse HEAD).Trim()
Invoke-Checked git @('-c', 'http.sslBackend=schannel', 'push', 'origin', 'HEAD:feat/persistent-settings-icons')
# The PR API can briefly report the previous head immediately after a push.
$headDeadline = (Get-Date).AddMinutes(2)
do {
    $prJson = gh pr view 2 --repo $repo --json headRefOid,isDraft,state
    if ($LASTEXITCODE -ne 0) { throw 'Could not read PR #2' }
    $pr = $prJson | ConvertFrom-Json
    if ($pr.state -ne 'OPEN') { throw "PR #2 is $($pr.state); expected OPEN" }
    if ($pr.headRefOid -eq $head) { break }
    if ((Get-Date) -gt $headDeadline) {
        throw "PR #2 head remains $($pr.headRefOid); expected $head. No merge or tag was performed."
    }
    Write-Host "Waiting for GitHub PR head to update to $head..."
    Start-Sleep -Seconds 5
} while ($true)
if ($pr.isDraft) { Invoke-Checked gh @('pr', 'ready', '2', '--repo', $repo) }
# Wait for both workflows on this exact head, including queued runs.
$deadline = (Get-Date).AddMinutes(45)
foreach ($workflow in @('ci.yml', 'companion.yml')) {
    do {
        $runs = @(gh run list --repo $repo --workflow $workflow --commit $head --event pull_request --limit 1 --json databaseId,status,conclusion | ConvertFrom-Json)
        if ($LASTEXITCODE -ne 0) { throw 'Could not read CI status' }
        if ($runs.Count -gt 0) { break }
        if ((Get-Date) -gt $deadline) { throw 'Timed out waiting for CI runs' }
        Start-Sleep -Seconds 10
    } while ($true)
    Invoke-Checked gh @('run', 'watch', "$($runs[0].databaseId)", '--repo', $repo, '--exit-status', '--interval', '15')
}
Invoke-Checked gh @('pr', 'merge', '2', '--repo', $repo, '--squash', '--match-head-commit', $head)
$pr = gh pr view 2 --repo $repo --json state,mergeCommit | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or $pr.state -ne 'MERGED' -or !$pr.mergeCommit.oid) { throw 'Merge not confirmed' }
$merge = $pr.mergeCommit.oid
# GitHub rejects an existing tag rather than replacing it.
Invoke-Checked gh @('api', "repos/$repo/git/refs", '-f', "ref=refs/tags/$tag", '-f', "sha=$merge")
$deadline = (Get-Date).AddMinutes(10)
do {
    $runs = @(gh run list --repo $repo --workflow release.yml --commit $merge --event push --limit 1 --json databaseId | ConvertFrom-Json)
    if ($LASTEXITCODE -ne 0) { throw 'Could not read release workflow' }
    if ($runs.Count -gt 0) { break }
    if ((Get-Date) -gt $deadline) { throw 'Tag exists; inspect release workflow before retrying' }
    Start-Sleep -Seconds 10
} while ($true)
Invoke-Checked gh @('run', 'watch', "$($runs[0].databaseId)", '--repo', $repo, '--exit-status', '--interval', '15')
$release = gh release view $tag --repo $repo --json assets,isDraft | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'Release unavailable' }
foreach ($asset in @('wristfoundry-v0.2.0.tar.gz', 'SHA256SUMS', 'wristfoundry-companion-windows-x64.zip', 'COMPANION-SHA256SUMS')) {
    if ($asset -notin $release.assets.name) { throw "Missing release asset: $asset" }
}
Invoke-Checked gh @('release', 'edit', $tag, '--repo', $repo, '--draft=false', '--latest')
Invoke-Checked gh @('release', 'view', $tag, '--repo', $repo, '--json', 'url,isDraft,tagName')
