[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$destination = Join-Path $root 'upstream\libfunctionpatcher'
$remote = 'https://github.com/wiiu-env/libfunctionpatcher.git'
$commit = '8e6f361ee5049f615e7651a17f4bc3784180f01f'

if (Test-Path -LiteralPath (Join-Path $destination '.git')) {
    $actual = (& git -C $destination rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $actual -ne $commit) {
        throw "Existing libfunctionpatcher checkout is not pinned to $commit"
    }
    Write-Host "libfunctionpatcher already pinned at $commit"
    return
}

New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force |
    Out-Null
& git clone --filter=blob:none --no-checkout $remote $destination
if ($LASTEXITCODE -ne 0) { throw 'Could not clone libfunctionpatcher.' }
& git -C $destination fetch --depth 1 origin $commit
if ($LASTEXITCODE -ne 0) { throw 'Could not fetch pinned libfunctionpatcher.' }
& git -C $destination checkout --detach $commit
if ($LASTEXITCODE -ne 0) { throw 'Could not check out pinned libfunctionpatcher.' }

Write-Host "Ready: libfunctionpatcher $commit"
