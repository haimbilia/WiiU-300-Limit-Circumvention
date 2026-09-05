[CmdletBinding()]
param(
    [switch]$Clean,
    [ValidateSet('LoadOnly', 'Validator', 'Production')]
    [string]$Stage = 'LoadOnly',
    [switch]$AcknowledgeHardwareRisk
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$plugin = Join-Path $root 'plugin'
$image = 'wiiu-title-limit-builder:2026-09-03'
$sourceDateEpoch = '1788307200'

& docker version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Docker Desktop is not available.' }

Write-Host "Building Docker image $image..."
& docker build --pull --provenance=false -f (Join-Path $plugin 'Dockerfile') -t $image $root
if ($LASTEXITCODE -ne 0) { throw 'Docker image build failed.' }

if ($Stage -ne 'LoadOnly' -and -not $AcknowledgeHardwareRisk) {
    throw "$Stage is quarantined after the 2026-09-02 boot failure. Re-run with -AcknowledgeHardwareRisk for offline development only."
}

if ($Stage -eq 'Production') {
    & (Join-Path $PSScriptRoot 'verify-menu-profile.ps1') | Out-Host
}

switch ($Stage) {
    'LoadOnly' {
        $mode = 'probe-load-only'
        $configuration = 'PROBE=load-only'
        $targetName = 'WiiUMenuTitleLimitProbeLoadOnly'
        $makeArguments = @('make', 'PROBE=load-only')
    }
    'Validator' {
        $mode = 'validator-unsafe'
        $configuration = 'DEBUG=1 PATCH=0'
        $targetName = 'WiiUMenuTitleLimit'
        $makeArguments = @('make', 'DEBUG=1')
    }
    'Production' {
        $mode = 'production-candidate-unsafe'
        $configuration = 'PATCH=1'
        $targetName = 'WiiUMenuTitleLimit'
        $makeArguments = @('make', 'PATCH=1')
    }
}

if ($Clean) {
    $cleanArguments = @('make') + $makeArguments[1..($makeArguments.Count - 1)] + 'clean'
    & docker run --rm --mount "type=bind,source=$plugin,target=/project" $image @cleanArguments
    if ($LASTEXITCODE -ne 0) { throw 'Plugin clean failed.' }
}

Write-Host "Building $mode WUPS plugin..."
& docker run --rm --env "SOURCE_DATE_EPOCH=$sourceDateEpoch" `
    --mount "type=bind,source=$plugin,target=/project" $image @makeArguments
if ($LASTEXITCODE -ne 0) { throw 'Plugin build failed.' }

$artifact = Join-Path $plugin "$targetName.wps"
$elf = Join-Path $plugin "$targetName.elf"
if (-not (Test-Path -LiteralPath $artifact)) { throw "Missing build artifact: $artifact" }
if (-not (Test-Path -LiteralPath $elf)) { throw "Missing ELF artifact: $elf" }

$inspection = $null
if ($Stage -eq 'LoadOnly' -or $Stage -eq 'Production') {
    $mount = "type=bind,source=$plugin,target=/project"
    $readElf = '/opt/devkitpro/devkitPPC/bin/powerpc-eabi-readelf'
    $sections = (& docker run --rm --mount $mount $image `
        $readElf -S "/project/$targetName.elf") -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect load-only ELF sections.' }
    $symbols = (& docker run --rm --mount $mount $image `
        $readElf -Ws "/project/$targetName.elf") -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect load-only ELF symbols.' }

    if ($sections -match '\.wups\.load') {
        throw "$Stage build unexpectedly contains a function-replacement section."
    }
    $forbiddenSymbols = if ($Stage -eq 'LoadOnly') {
        @('MCP_', 'FunctionPatcher', 'KernelCopyData', 'OSDynLoad_GetNumberOfRPLs',
          'OSDynLoad_GetRPLInfo', 'DCInvalidateRange', 'ICInvalidateRange')
    } else {
        @('real_MCP_', 'MCP_TitleCount', 'MCP_TitleList',
          'MCP_TitleListByAppType', 'MCP_TitleListByUniqueId',
          'MCP_TitleListByDevice', 'MCP_TitleListByDeviceType',
          'MCP_TitleListByAppAndDevice', 'MCP_TitleListByAppAndDeviceType',
          'MCP_TitleListByUniqueIdAndIndexedDeviceAndAppType')
    }
    foreach ($symbol in $forbiddenSymbols) {
        if ($symbols.Contains($symbol, [StringComparison]::Ordinal)) {
            throw "$Stage build unexpectedly references $symbol."
        }
    }
    if ($sections -notmatch '\.wups\.hooks' -or $symbols -notmatch 'wups_hooks_init_plugin') {
        throw "$Stage build is missing its expected initialization hook."
    }
    $metadata = (& docker run --rm --mount $mount $image `
        $readElf --string-dump=.wups.meta "/project/$targetName.elf") -join "`n"
    if ($LASTEXITCODE -ne 0) { throw "Could not inspect $Stage ELF metadata." }
    if ($Stage -eq 'Production') {
        foreach ($requiredMetadata in @('name=Wii U Menu Title Limit',
                                        'version=v1.0.0-rc18', 'wups=0.9.1')) {
            if (-not $metadata.Contains($requiredMetadata, [StringComparison]::Ordinal)) {
                throw "Production build is missing metadata $requiredMetadata."
            }
        }
        foreach ($requiredSymbol in @('KernelCopyData', 'OSDynLoad_GetNumberOfRPLs',
                                      'OSDynLoad_GetRPLInfo', 'DCFlushRange',
                                      'ICInvalidateRange', 'FunctionPatcher_InitLibrary',
                                      'FunctionPatcher_AddFunctionPatch',
                                      'FunctionPatcher_RemoveFunctionPatch')) {
            if (-not $symbols.Contains($requiredSymbol, [StringComparison]::Ordinal)) {
                throw "Production build is missing required symbol $requiredSymbol."
            }
        }
    }
    $inspection = if ($Stage -eq 'LoadOnly') {
        [ordered]@{
            functionReplacementSection = $false
            customHook = 'INITIALIZE_PLUGIN (empty)'
            forbiddenSymbolCount = 0
        }
    } else {
        [ordered]@{
            functionReplacementSection = $false
            customHooks = @('INITIALIZE_PLUGIN', 'DEINITIALIZE_PLUGIN',
                            'ON_APPLICATION_START', 'ON_APPLICATION_ENDS')
            forbiddenInterceptionSymbolCount = 0
            requiredRuntimeSymbols = @('KernelCopyData', 'OSDynLoad_GetNumberOfRPLs',
                                       'OSDynLoad_GetRPLInfo', 'DCFlushRange',
                                       'ICInvalidateRange', 'FunctionPatcher_InitLibrary',
                                       'FunctionPatcher_AddFunctionPatch',
                                       'FunctionPatcher_RemoveFunctionPatch')
            metadata = @('name=Wii U Menu Title Limit', 'version=v1.0.0-rc18',
                         'wups=0.9.1')
        }
    }
}

$lab = Join-Path $plugin 'lab'
New-Item -ItemType Directory -Path $lab -Force | Out-Null
$labArtifact = Join-Path $lab "$targetName-$mode.wps"
Move-Item -LiteralPath $artifact -Destination $labArtifact -Force

$artifactInfo = Get-Item -LiteralPath $labArtifact
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $labArtifact).Hash.ToLowerInvariant()
$imageId = (& docker image inspect $image --format '{{.Id}}').Trim()
if ($LASTEXITCODE -ne 0 -or -not $imageId) { throw 'Could not identify the builder image.' }

$lock = [ordered]@{
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    mode = $mode
    configuration = $configuration
    sourceDateEpoch = $sourceDateEpoch
    artifact = [ordered]@{
        path = "plugin/lab/$targetName-$mode.wps"
        bytes = $artifactInfo.Length
        sha256 = $hash
    }
    builder = [ordered]@{
        image = $image
        imageId = $imageId
    }
    inspection = $inspection
}
$lockPath = Join-Path $root "tooling\plugin-build-lock-$mode.json"
New-Item -ItemType Directory -Path (Split-Path -Parent $lockPath) -Force |
    Out-Null
$lock | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $lockPath -Encoding utf8

Write-Host "Built plugin/lab/$targetName-$mode.wps"
Write-Host "SHA-256: $hash"
Write-Host "Build lock: tooling/plugin-build-lock-$mode.json"
