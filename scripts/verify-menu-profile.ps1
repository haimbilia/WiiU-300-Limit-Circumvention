[CmdletBinding()]
param(
    [string]$Profile = 'analysis\signatures\usa-v277-b67deb8fb368.json',
    [string]$MenuBinary = 'private-dumps\menu\men.rpx',
    [string]$GhidraReport = 'analysis\notes\generated\menu_b67deb8fb368-patch-profile-sites.json'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $root $Path
}

$profilePath = Resolve-ProjectPath $Profile
$binaryPath = Resolve-ProjectPath $MenuBinary
$reportPath = Resolve-ProjectPath $GhidraReport

foreach ($path in $profilePath, $binaryPath) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required profile input is missing: $path"
    }
}

$profileData = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$report = if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
    Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
} else {
    $null
}
$runtimeSource = Get-Content -LiteralPath (Join-Path $root 'plugin\src\patches\patch_runtime.cpp') -Raw
$binary = Get-Item -LiteralPath $binaryPath
$binaryHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $binaryPath).Hash.ToLowerInvariant()

if ($binary.Length -ne [int64]$profileData.menRpx.bytes) {
    throw "men.rpx size mismatch: expected $($profileData.menRpx.bytes), found $($binary.Length)"
}
if ($binaryHash -ne $profileData.menRpx.sha256) {
    throw "men.rpx SHA-256 mismatch: expected $($profileData.menRpx.sha256), found $binaryHash"
}
if ($report -and $report.program.sha256 -and $report.program.sha256 -ne $binaryHash) {
    throw "Ghidra report hash mismatch: expected $binaryHash, found $($report.program.sha256)"
}

$requiredRuntimeTokens = @(
    "0x$($profileData.titleId)ULL",
    "= $($profileData.titleVersion);",
    "0x$($profileData.canonicalTextBase)",
    "= $($profileData.targetSlotCapacity);",
    ('"' + $profileData.name + '"')
)
foreach ($token in $requiredRuntimeTokens) {
    if (-not $runtimeSource.Contains($token, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Plugin runtime is missing profile identity token $token"
    }
}

$textBase = [Convert]::ToUInt32($profileData.canonicalTextBase, 16)
$validatedWords = 0
foreach ($patch in $profileData.patches) {
    foreach ($token in $patch.canonicalAddress, $patch.original, $patch.replacement) {
        if ($runtimeSource -notmatch "0x$([regex]::Escape($token))") {
            throw "Plugin runtime is missing profile token 0x$token for $($patch.name)"
        }
    }

    $site = [Convert]::ToUInt32($patch.canonicalAddress, 16)
    $calculatedOffset = $site - $textBase
    $declaredOffset = [Convert]::ToUInt32($patch.textOffset, 16)
    if ($calculatedOffset -ne $declaredOffset) {
        throw "Text offset mismatch for $($patch.name)"
    }

    foreach ($signatureWord in $patch.signature) {
        if (-not $runtimeSource.Contains("0x$($signatureWord.word)", [StringComparison]::OrdinalIgnoreCase)) {
            throw "Plugin runtime is missing signature word 0x$($signatureWord.word) for $($patch.name)"
        }
        if (-not $report) {
            $validatedWords++
            continue
        }
        $function = $report.functions | Where-Object name -eq $patch.function | Select-Object -First 1
        if (-not $function) { throw "Function missing from Ghidra report: $($patch.function)" }
        $address = [uint32]([int64]$site + [int64]$signatureWord.delta)
        $addressText = $address.ToString('x8')
        $instruction = $function.instructions |
            Where-Object address -eq $addressText | Select-Object -First 1
        if (-not $instruction) {
            throw "Instruction $addressText missing for $($patch.name)"
        }
        if ($instruction.bytes.ToLowerInvariant() -ne $signatureWord.word) {
            throw "Signature mismatch at $addressText for $($patch.name): expected $($signatureWord.word), found $($instruction.bytes)"
        }
        $validatedWords++
    }

    if ($report) {
        $function = $report.functions | Where-Object name -eq $patch.function | Select-Object -First 1
        $targetInstruction = $function.instructions |
            Where-Object address -eq $patch.canonicalAddress | Select-Object -First 1
        if ($targetInstruction.bytes.ToLowerInvariant() -ne $patch.original) {
            throw "Original patch word mismatch for $($patch.name)"
        }
    }
}

[pscustomobject]@{
    Profile = $profileData.name
    TitleId = $profileData.titleId
    TitleVersion = $profileData.titleVersion
    BinarySha256 = $binaryHash
    PatchSites = $profileData.patches.Count
    SignatureWords = $validatedWords
    TargetSlots = $profileData.targetSlotCapacity
    OfflineInstructionReport = [bool]$report
    Valid = $true
}
