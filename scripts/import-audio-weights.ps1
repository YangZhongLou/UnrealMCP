#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory, Position = 0, HelpMessage = 'Directory containing pre-downloaded audio model weights.')]
    [string]$SourceDir,

    [Parameter(HelpMessage = 'Create directory junctions/symlinks instead of copying files.')]
    [switch]$Symlink,

    [Parameter(HelpMessage = 'Skip ACE-Step weights.')]
    [switch]$SkipAceStep,

    [Parameter(HelpMessage = 'Skip Stable Audio Open weights.')]
    [switch]$SkipStableAudio,

    [Parameter(HelpMessage = 'Skip MMAudio weights.')]
    [switch]$SkipMMAudio,

    [Parameter(HelpMessage = 'Update audio_server/.env with the imported ACE_STEP_CHECKPOINT_PATH.')]
    [switch]$UpdateEnv,

    [Parameter(HelpMessage = 'Overwrite existing files/directories without prompting.')]
    [switch]$Force
)

<#
.SYNOPSIS
    Imports pre-downloaded audio model weights into the UnrealMCP audio_server layout.
.DESCRIPTION
    Validates a source directory of ACE-Step, Stable Audio Open, and MMAudio weights
    and copies/symlinks them to the locations expected by Plugins/UnrealMCP/audio_server/main.py.
    Use -WhatIf to preview changes. The script refuses to write to system directories.
.PARAMETER SourceDir
    Root directory where weights were pre-downloaded. Expected layout:
        ACE-Step/checkpoints/{music_dcae_f8c8,music_vocoder,ace_step_transformer,umt5-base}
        stable-audio-open/models--stabilityai--stable-audio-open-1.0/
        MMAudio/weights/*.pth
        MMAudio/ext_weights/*.pth
.EXAMPLE
    .\Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights
.EXAMPLE
    .\Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -WhatIf
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Console helpers
# ---------------------------------------------------------------------------
function Write-Info    { param([string]$Message) Write-Host $Message -ForegroundColor Cyan }
function Write-Ok      { param([string]$Message) Write-Host $Message -ForegroundColor Green }
function Write-Warn    { param([string]$Message) Write-Host $Message -ForegroundColor Yellow }
function Write-Err     { param([string]$Message) Write-Host $Message -ForegroundColor Red }
function Write-Header  {
    param([string]$Message)
    Write-Host ''
    Write-Host ('=' * 60) -ForegroundColor Cyan
    Write-Host $Message -ForegroundColor Cyan
    Write-Host ('=' * 60) -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# Path resolution
# ---------------------------------------------------------------------------
$ScriptDir    = $PSScriptRoot
$PluginRoot   = Resolve-Path (Join-Path $ScriptDir '..')
$ThirdParty   = Join-Path $PluginRoot 'third_party'
$AudioServer  = Join-Path $PluginRoot 'audio_server'
$EnvPath      = Join-Path $AudioServer '.env'

$AceStepRepo = Join-Path $ThirdParty 'ACE-Step'
$MMAudioRepo = Join-Path $ThirdParty 'MMAudio'

# HF cache resolution (huggingface_hub uses HF_HOME, then default user cache)
$HfHome = if ($env:HF_HOME) { $env:HF_HOME } elseif ($env:TRANSFORMERS_CACHE) { $env:TRANSFORMERS_CACHE } else { Join-Path $env:USERPROFILE '.cache\huggingface' }
$HfHubDir = Join-Path $HfHome 'hub'

# Destination paths
$AceStepDest          = Join-Path $AceStepRepo 'checkpoints'
$StableAudioCacheName = 'models--stabilityai--stable-audio-open-1.0'
$StableAudioDest      = Join-Path $HfHubDir $StableAudioCacheName
$MMAudioWeightsDest   = Join-Path $MMAudioRepo 'weights'
$MMAudioExtWeightsDest = Join-Path $MMAudioRepo 'ext_weights'
# MMAudio's code resolves ./weights and ./ext_weights relative to the process CWD,
# so audio_server needs them too when launched from its own folder.
$ServerWeightsDest    = Join-Path $AudioServer 'weights'
$ServerExtWeightsDest = Join-Path $AudioServer 'ext_weights'

# ---------------------------------------------------------------------------
# Safety: refuse to write to obvious system directories
# ---------------------------------------------------------------------------
$SystemPrefixes = @(
    (Join-Path $env:SystemRoot '').TrimEnd('\'),
    'C:\Windows',
    'C:\Program Files',
    'C:\Program Files (x86)',
    'C:\$Recycle.Bin',
    'C:\ProgramData'
)

function Assert-SafePath {
    param([Parameter(Mandatory)][string]$Path)

    $normalized = (Resolve-Path $Path -ErrorAction SilentlyContinue).Path
    if (-not $normalized) { $normalized = $Path }
    $normalized = $normalized.TrimEnd('\')

    foreach ($bad in $SystemPrefixes) {
        if ($normalized -ieq $bad -or $normalized.StartsWith("$bad\", [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to write to system directory: $Path"
        }
    }
}

# ---------------------------------------------------------------------------
# Source discovery
# ---------------------------------------------------------------------------
function Find-DirectoryByChild {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string[]]$ChildNames,
        [switch]$AllRequired
    )
    $rootObj = Get-Item $Root
    # Direct match first
    $children = Get-ChildItem -Path $Root -Directory -ErrorAction SilentlyContinue
    $allPresent = ($ChildNames | ForEach-Object { Test-Path (Join-Path $Root $_) }) -notcontains $false
    if ($AllRequired -and $allPresent) { return $Root }
    if (-not $AllRequired -and ($ChildNames | Where-Object { Test-Path (Join-Path $Root $_) })) { return $Root }

    # Search one level deeper
    foreach ($dir in $children) {
        $allPresent = ($ChildNames | ForEach-Object { Test-Path (Join-Path $dir.FullName $_) }) -notcontains $false
        if ($AllRequired -and $allPresent) { return $dir.FullName }
        if (-not $AllRequired -and ($ChildNames | Where-Object { Test-Path (Join-Path $dir.FullName $_) })) { return $dir.FullName }
    }
    return $null
}

function Find-AceStepSource {
    param([string]$Root)
    $required = @('music_dcae_f8c8', 'music_vocoder', 'ace_step_transformer', 'umt5-base')

    # Search up to three levels deep for a 'checkpoints' directory that contains the required subdirs.
    $candidates = Get-ChildItem -Path $Root -Directory -Recurse -Depth 3 -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq 'checkpoints' } |
        Where-Object {
            $dir = $_
            ($required | ForEach-Object { Test-Path (Join-Path $dir.FullName $_) -PathType Container }) -notcontains $false
        } |
        Select-Object -First 1

    if ($candidates) { return $candidates.FullName }

    # Fallback: search for a directory that directly contains the required subdirs.
    $base = Find-DirectoryByChild -Root $Root -ChildNames $required -AllRequired
    if ($base) { return $base }
    return $null
}

function Find-StableAudioSource {
    param([string]$Root)
    $name = 'models--stabilityai--stable-audio-open-1.0'
    # Direct match
    $direct = Join-Path $Root $name
    if (Test-Path $direct) { return $direct }
    # One level deeper, possibly under 'hub' or 'stable-audio-open'
    $candidates = Get-ChildItem -Path $Root -Directory -Recurse -Depth 2 -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq $name } |
        Select-Object -First 1
    if ($candidates) { return $candidates.FullName }
    return $null
}

function Find-MMAudioSource {
    param([string]$Root)
    # Look for weights/ and ext_weights/ one level deep
    $candidates = Get-ChildItem -Path $Root -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            (Test-Path (Join-Path $_.FullName 'weights')) -and
            (Test-Path (Join-Path $_.FullName 'ext_weights'))
        } |
        Select-Object -First 1
    if ($candidates) { return $candidates.FullName }
    # Root itself
    if ((Test-Path (Join-Path $Root 'weights')) -and (Test-Path (Join-Path $Root 'ext_weights'))) { return $Root }
    return $null
}

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
function Test-AceStepLayout {
    param([string]$Path)
    $required = @('music_dcae_f8c8', 'music_vocoder', 'ace_step_transformer', 'umt5-base')
    $missing = $required | Where-Object { -not (Test-Path (Join-Path $Path $_) -PathType Container) }
    return $missing
}

function Test-MMAudioLayout {
    param([string]$Path)
    $weightsDir = Join-Path $Path 'weights'
    $extDir = Join-Path $Path 'ext_weights'
    $report = @{ Missing = @(); Found = @() }

    if (-not (Test-Path $weightsDir)) { $report.Missing += 'weights/'; return $report }
    if (-not (Test-Path $extDir)) { $report.Missing += 'ext_weights/'; return $report }

    $modelFile = Get-ChildItem -Path $weightsDir -Filter 'mmaudio_*.pth' -File | Select-Object -First 1
    if (-not $modelFile) { $report.Missing += 'weights/mmaudio_*.pth' }
    else { $report.Found += "weights/$($modelFile.Name)" }

    foreach ($f in @('v1-44.pth', 'synchformer_state_dict.pth')) {
        if (Test-Path (Join-Path $extDir $f) -PathType Leaf) { $report.Found += "ext_weights/$f" }
        else { $report.Missing += "ext_weights/$f" }
    }

    # best_netG.pt is optional (used for 16kHz variants); report but do not require
    if (Test-Path (Join-Path $extDir 'best_netG.pt') -PathType Leaf) { $report.Found += 'ext_weights/best_netG.pt' }

    return $report
}

# ---------------------------------------------------------------------------
# MD5 verification
# ---------------------------------------------------------------------------
function Get-FileMd5 {
    param([Parameter(Mandatory)][string]$Path)
    $stream = [System.IO.FileStream]::new($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $md5 = [System.Security.Cryptography.MD5]::Create()
        $bytes = $md5.ComputeHash($stream)
        return ([System.BitConverter]::ToString($bytes) -replace '-', '').ToLowerInvariant()
    }
    finally { $stream.Dispose() }
}

function Test-Md5Sidecar {
    param([Parameter(Mandatory)][string]$FilePath)
    $md5Path = "$FilePath.md5"
    if (-not (Test-Path $md5Path)) { return @{ Ok = $true; Checked = $false; Message = 'No .md5 sidecar' } }

    $expected = (Get-Content $md5Path -TotalCount 1).Trim()
    if ($expected -match '^([a-fA-F0-9]{32})') { $expected = $matches[1].ToLowerInvariant() }
    else { return @{ Ok = $false; Checked = $true; Message = "Invalid .md5 format: $md5Path" } }

    $actual = Get-FileMd5 -Path $FilePath
    if ($actual -eq $expected) { return @{ Ok = $true; Checked = $true; Message = "MD5 OK" } }
    return @{ Ok = $false; Checked = $true; Message = "MD5 mismatch (expected $expected, got $actual)" }
}

# ---------------------------------------------------------------------------
# Copy / link helpers
# ---------------------------------------------------------------------------
function Invoke-CopyItemTree {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [switch]$Force
    )
    if (-not (Test-Path $Destination)) {
        New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    }

    $items = Get-ChildItem -Path $Source -Recurse -File
    foreach ($item in $items) {
        $relative = $item.FullName.Substring($Source.Length).TrimStart('\')
        $target = Join-Path $Destination $relative
        $targetDir = Split-Path $target -Parent
        if (-not (Test-Path $targetDir)) { New-Item -ItemType Directory -Force -Path $targetDir | Out-Null }

        $copy = $true
        if (Test-Path $target) {
            if ($Force -or $PSCmdlet.ShouldContinue("Overwrite existing file '$target'?", 'Confirm overwrite')) {
                Remove-Item $target -Force
            }
            else {
                $copy = $false
            }
        }
        if ($copy) {
            Copy-Item -Path $item.FullName -Destination $target -Force
        }
    }
}

function Invoke-LinkOrCopyDirectory {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [switch]$Symlink,
        [switch]$Force
    )
    Assert-SafePath -Path $Destination

    if (Test-Path $Destination) {
        if ($Force -or $PSCmdlet.ShouldContinue("Remove existing '$Destination' and replace it?", 'Confirm replacement')) {
            Remove-Item -Recurse -Force $Destination
        }
        else {
            Write-Warn "Skipped: $Destination already exists."
            return
        }
    }

    if ($Symlink) {
        try {
            # Directory junctions do not require admin/Developer Mode on Windows NTFS.
            New-Item -ItemType Junction -Path $Destination -Target $Source | Out-Null
            Write-Ok "Created junction: $Destination -> $Source"
            return
        }
        catch {
            Write-Warn "Junction failed ($($_.Exception.Message)); falling back to copy."
        }
    }

    Invoke-CopyItemTree -Source $Source -Destination $Destination -Force:$Force
    Write-Ok "Copied: $Source -> $Destination"
}

function Invoke-LinkOrCopyFile {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [switch]$Symlink,
        [switch]$Force
    )
    Assert-SafePath -Path $Destination
    $destDir = Split-Path $Destination -Parent
    if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Force -Path $destDir | Out-Null }

    if (Test-Path $Destination) {
        if ($Force -or $PSCmdlet.ShouldContinue("Overwrite existing file '$Destination'?", 'Confirm overwrite')) {
            Remove-Item $Destination -Force
        }
        else {
            Write-Warn "Skipped: $Destination already exists."
            return
        }
    }

    if ($Symlink) {
        try {
            New-Item -ItemType SymbolicLink -Path $Destination -Target $Source | Out-Null
            Write-Ok "Created symlink: $Destination -> $Source"
            return
        }
        catch {
            Write-Warn "Symbolic link failed ($($_.Exception.Message)); falling back to copy."
        }
    }

    Copy-Item -Path $Source -Destination $Destination -Force
    Write-Ok "Copied: $Source -> $Destination"
}

# ---------------------------------------------------------------------------
# .env update
# ---------------------------------------------------------------------------
function Update-DotEnv {
    param(
        [Parameter(Mandatory)][string]$EnvPath,
        [Parameter(Mandatory)][string]$AceCheckpointPath
    )
    $forwardPath = $AceCheckpointPath.Replace('\', '/')
    $lines = if (Test-Path $EnvPath) { @(Get-Content $EnvPath) } else { @() }
    $found = $false
    $newLines = foreach ($line in $lines) {
        if ($line -match '^\s*ACE_STEP_CHECKPOINT_PATH\s*=') {
            "ACE_STEP_CHECKPOINT_PATH=$forwardPath"
            $found = $true
        }
        else { $line }
    }
    if (-not $found) {
        $newLines += ""
        $newLines += "# Added by import-audio-weights.ps1"
        $newLines += "ACE_STEP_CHECKPOINT_PATH=$forwardPath"
    }

    Set-Content -Path $EnvPath -Value $newLines -Encoding UTF8 -Force
    Write-Ok "Updated $EnvPath with ACE_STEP_CHECKPOINT_PATH=$forwardPath"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
Write-Header 'UnrealMCP Audio Weights Import'

if (-not (Test-Path $SourceDir -PathType Container)) {
    Write-Err "Source directory not found: $SourceDir"
    exit 1
}
$SourceRoot = (Resolve-Path $SourceDir).Path
Write-Info "Source: $SourceRoot"
Write-Info "Plugin root: $PluginRoot"
Write-Info "HF cache: $HfHubDir"
Write-Host ''

# Discovery
$aceSource = if (-not $SkipAceStep) { Find-AceStepSource -Root $SourceRoot } else { $null }
$saSource  = if (-not $SkipStableAudio) { Find-StableAudioSource -Root $SourceRoot } else { $null }
$mmaSource = if (-not $SkipMMAudio) { Find-MMAudioSource -Root $SourceRoot } else { $null }

# Validation / reporting
$plan = @()
$warnings = [System.Collections.Generic.List[string]]::new()

if (-not $SkipAceStep) {
    Write-Header 'ACE-Step'
    if (-not $aceSource) {
        Write-Warn '  ACE-Step checkpoints not found in source.'
        Write-Info '  Expected: ACE-Step/checkpoints/{music_dcae_f8c8,music_vocoder,ace_step_transformer,umt5-base}'
        $warnings.Add('ACE-Step: no valid checkpoint tree found.')
    }
    else {
        Write-Info "  Found checkpoint tree: $aceSource"
        $missing = Test-AceStepLayout -Path $aceSource
        if ($missing) {
            Write-Warn "  Missing subdirectories: $($missing -join ', ')"
            $warnings.Add("ACE-Step: missing $($missing -join ', ').")
        }
        else {
            Write-Ok '  Layout looks valid.'
            $plan += [pscustomobject]@{ Model = 'ACE-Step'; Source = $aceSource; Destination = $AceStepDest; Kind = 'directory' }
        }
    }
}

if (-not $SkipStableAudio) {
    Write-Header 'Stable Audio Open'
    if (-not $saSource) {
        Write-Warn '  Stable Audio Open HuggingFace cache not found in source.'
        Write-Info "  Expected: stable-audio-open/$StableAudioCacheName/ (HF hub snapshot)"
        $warnings.Add('Stable Audio Open: no HF cache snapshot found.')
    }
    else {
        Write-Info "  Found HF cache snapshot: $saSource"
        $plan += [pscustomobject]@{ Model = 'Stable Audio Open'; Source = $saSource; Destination = $StableAudioDest; Kind = 'directory' }
    }
}

if (-not $SkipMMAudio) {
    Write-Header 'MMAudio'
    if (-not $mmaSource) {
        Write-Warn '  MMAudio weights not found in source.'
        Write-Info '  Expected: MMAudio/weights/mmaudio_*.pth and MMAudio/ext_weights/{v1-44.pth,synchformer_state_dict.pth}'
        $warnings.Add('MMAudio: no weights/ext_weights tree found.')
    }
    else {
        Write-Info "  Found MMAudio tree: $mmaSource"
        $report = Test-MMAudioLayout -Path $mmaSource
        if ($report.Missing) {
            Write-Warn "  Missing: $($report.Missing -join ', ')"
            $warnings.Add("MMAudio: missing $($report.Missing -join ', ').")
        }
        if ($report.Found) {
            Write-Ok "  Found: $($report.Found -join ', ')"
            $plan += [pscustomobject]@{ Model = 'MMAudio (repo)'; Source = (Join-Path $mmaSource 'weights'); Destination = $MMAudioWeightsDest; Kind = 'directory' }
            $plan += [pscustomobject]@{ Model = 'MMAudio (repo ext)'; Source = (Join-Path $mmaSource 'ext_weights'); Destination = $MMAudioExtWeightsDest; Kind = 'directory' }
            $plan += [pscustomobject]@{ Model = 'MMAudio (server)'; Source = (Join-Path $mmaSource 'weights'); Destination = $ServerWeightsDest; Kind = 'directory' }
            $plan += [pscustomobject]@{ Model = 'MMAudio (server ext)'; Source = (Join-Path $mmaSource 'ext_weights'); Destination = $ServerExtWeightsDest; Kind = 'directory' }
        }
    }
}

Write-Header 'Import plan'
if (-not $plan) {
    Write-Warn 'Nothing to import. Review the warnings above and verify your source directory layout.'
    exit 1
}

foreach ($item in $plan) {
    Write-Info "  [$($item.Model)] $($item.Source)"
    Write-Host "     -> $($item.Destination)" -ForegroundColor Gray
}

if ($warnings.Count -gt 0) {
    Write-Header 'Warnings'
    $warnings | ForEach-Object { Write-Warn "  $_" }
}

if (-not $PSCmdlet.ShouldProcess($SourceRoot, 'Import audio model weights')) {
    exit 0
}

# Execute
Write-Header 'Importing'
$importedAcePath = $null
foreach ($item in $plan) {
    if ($item.Kind -eq 'directory') {
        Invoke-LinkOrCopyDirectory -Source $item.Source -Destination $item.Destination -Symlink:$Symlink -Force:$Force
    }
    else {
        Invoke-LinkOrCopyFile -Source $item.Source -Destination $item.Destination -Symlink:$Symlink -Force:$Force
    }
    if ($item.Model -eq 'ACE-Step') { $importedAcePath = $item.Destination }
}

# Checksum pass (source sidecars only)
Write-Header 'Verifying checksums'
$md5Files = Get-ChildItem -Path $SourceRoot -Recurse -Filter '*.md5' -File -ErrorAction SilentlyContinue
if (-not $md5Files) {
    Write-Info '  No .md5 sidecar files found in source; skipping checksum verification.'
}
else {
    $failures = 0
    foreach ($md5File in $md5Files) {
        $targetFile = $md5File.FullName -replace '\.md5$', ''
        if (-not (Test-Path $targetFile)) {
            Write-Warn "  Missing file for .md5 sidecar: $targetFile"
            $failures++
            continue
        }
        $result = Test-Md5Sidecar -FilePath $targetFile
        if ($result.Ok) {
            Write-Ok "  $($md5File.Name): $($result.Message)"
        }
        else {
            Write-Err "  $($md5File.Name): $($result.Message)"
            $failures++
        }
    }
    if ($failures -gt 0) {
        Write-Warn "  $failures checksum problem(s) detected. Re-download the affected files if the model fails to load."
    }
}

# Update .env
if ($importedAcePath) {
    if ($UpdateEnv) {
        if ($PSCmdlet.ShouldProcess($EnvPath, 'Update ACE_STEP_CHECKPOINT_PATH')) {
            Update-DotEnv -EnvPath $EnvPath -AceCheckpointPath $importedAcePath
        }
    }
    else {
        Write-Info "ACE_STEP_CHECKPOINT_PATH should be set to: $($importedAcePath.Replace('\', '/'))"
        Write-Info "Re-run with -UpdateEnv to update $EnvPath automatically."
    }
}

Write-Header 'Done'
Write-Info 'Next steps:'
if ($StableAudioDest) { Write-Info "  - Ensure HF_HOME is set to '$($HfHome.Replace('\', '/'))' when starting the server, or leave it as the default user cache." }
Write-Info '  - Start the server: python Plugins/UnrealMCP/audio_server/main.py'
Write-Info '  - Test with: POST /health'
