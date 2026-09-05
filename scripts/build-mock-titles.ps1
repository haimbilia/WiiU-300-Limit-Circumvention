[CmdletBinding()]
param(
    [ValidateRange(1, 999)]
    [int]$Count = 300
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $root 'test-tools\mock-title-rpx'
$output = Join-Path $root 'plugin\lab\mock-titles'
$image = 'wiiu-title-limit-builder:2026-09-03'

& (Join-Path $PSScriptRoot 'generate-mock-title-assets.ps1') | Format-Table -AutoSize

& docker version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Docker Desktop is not available.' }

& docker build --pull=false --provenance=false `
    -f (Join-Path $root 'plugin\Dockerfile') -t $image $root
if ($LASTEXITCODE -ne 0) { throw 'Mock-title builder image failed.' }

$mount = "type=bind,source=$source,target=/project"
& docker run --rm --mount $mount $image make clean
if ($LASTEXITCODE -ne 0) { throw 'Mock-title clean failed.' }
& docker run --rm --env 'SOURCE_DATE_EPOCH=1788307200' --mount $mount $image make
if ($LASTEXITCODE -ne 0) { throw 'Mock-title build failed.' }

$baseBundle = Join-Path $source 'MockTitle.wuhb'
if (-not (Test-Path -LiteralPath $baseBundle)) { throw "Missing $baseBundle" }

$resolvedLab = (Resolve-Path (Join-Path $root 'plugin\lab')).Path
if (Test-Path -LiteralPath $output) {
    $resolvedOutput = (Resolve-Path -LiteralPath $output).Path
    if (-not $resolvedOutput.StartsWith($resolvedLab + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to replace unexpected output path: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $output | Out-Null

function Get-HomebrewPathHash([string]$Path) {
    [uint64]$hash = 0
    foreach ($byte in [Text.Encoding]::ASCII.GetBytes($Path)) {
        $hash = (($hash * 37) + $byte) -band 0xFFFFFFFFL
    }
    return [uint32]$hash
}

$entries = @()
$seen = @{}
for ($index = 1; $index -le $Count; $index++) {
    $name = 'TitleLimitTest{0:D3}.wuhb' -f $index
    $relativePath = "wiiu/apps/title-limit-test/$name"
    $lowerTitleId = Get-HomebrewPathHash $relativePath
    $titleId = '0005000F{0:X8}' -f $lowerTitleId
    if ($seen.ContainsKey($titleId)) { throw "Synthetic title-ID collision: $titleId" }
    $seen[$titleId] = $true

    $destination = Join-Path $output $name
    Copy-Item -LiteralPath $baseBundle -Destination $destination
    $entries += [ordered]@{
        index = $index
        file = $name
        sdPath = "wiiu/apps/title-limit-test/$name"
        syntheticTitleId = $titleId
    }
}

$baseHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $baseBundle).Hash.ToLowerInvariant()
$manifest = [ordered]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
    count = $Count
    baseBundle = [ordered]@{
        bytes = (Get-Item -LiteralPath $baseBundle).Length
        sha256 = $baseHash
    }
    remoteDirectory = 'wiiu/apps/title-limit-test'
    titleIdDerivation = '0005000F00000000 OR hash37(relative SD path)'
    entries = $entries
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8

Write-Host "Built $Count mock titles in $output"
Write-Host "Each copy SHA-256: $baseHash"
Write-Host "Total bytes: $((Get-ChildItem -LiteralPath $output -Filter '*.wuhb' | Measure-Object Length -Sum).Sum)"
