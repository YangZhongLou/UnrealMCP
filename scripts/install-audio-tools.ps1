#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$Force,
    [switch]$SkipRepoClone
)

<#
.SYNOPSIS
    One-click installer for UnrealMCP audio-designer AI tools.
.DESCRIPTION
    Installs ACE-Step, Stable Audio Open and MMAudio under Plugins/UnrealMCP/third_party/,
    creates a conda environment named 'ai-audio' (or a venv fallback), installs PyTorch,
    and writes an audio_server/.env file.
.PARAMETER Force
    Remove and re-clone existing third_party directories.
.PARAMETER SkipRepoClone
    Skip cloning/updating repositories (useful to re-run environment setup only).
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
function Write-Header  { param([string]$Message)
    Write-Host ''
    Write-Host ('=' * 60) -ForegroundColor Cyan
    Write-Host $Message -ForegroundColor Cyan
    Write-Host ('=' * 60) -ForegroundColor Cyan
}

function Test-Command {
    param([string]$Command)
    $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

function Invoke-Step {
    param(
        [Parameter(Mandatory)]
        [scriptblock]$ScriptBlock,
        [string]$Description,
        [switch]$IgnoreExitCode
    )
    Write-Info "  > $Description"
    & $ScriptBlock
    if (-not $IgnoreExitCode -and $LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
        throw "Step failed: $Description (exit code $LASTEXITCODE)"
    }
}

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------
$PluginRoot   = Resolve-Path (Join-Path $PSScriptRoot '..')
$ThirdParty   = Join-Path $PluginRoot 'third_party'
$AudioServer  = Join-Path $PluginRoot 'audio_server'
$OutputDir    = Join-Path $AudioServer 'output'
$VenvPath     = Join-Path $PluginRoot '.venv'

# PyTorch wheel index. CUDA 12.6 is supported by drivers >= 560.94 and keeps
# Stable Audio Open from pulling a CPU-only torch wheel.
$CudaIndexUrl = 'https://download.pytorch.org/whl/cu126'

$Repos = @(
    @{
        Name        = 'ACE-Step'
        Url         = 'https://github.com/ace-step/ACE-Step.git'
        DirName     = 'ACE-Step'
        InstallArgs = @('install', '-e', '.')
        VerifyCmd   = 'acestep --help'
        Notes       = 'Music generation (BGM / themes / lyrics).'
    },
    @{
        Name        = 'Stable Audio Open'
        Url         = 'https://github.com/Stability-AI/stable-audio-tools.git'
        DirName     = 'stable-audio-tools'
        InstallArgs = @('install', '-e', '.[train,ui]', '--index-url', $CudaIndexUrl)
        FallbackArgs = @('install', '-e', '.', '--index-url', $CudaIndexUrl)
        VerifyCmd   = 'python run_gradio.py --pretrained-name stabilityai/stable-audio-open-1.0'
        Notes       = 'SFX / ambient / short-loop generation.'
    },
    @{
        Name        = 'MMAudio'
        Url         = 'https://github.com/hkchengrex/MMAudio.git'
        DirName     = 'MMAudio'
        InstallArgs = @('install', '-e', '.')
        VerifyCmd   = 'python demo.py --prompt "coffee shop ambiance" --duration 8'
        Notes       = 'Video/text-to-audio Foley.'
    }
)

# ---------------------------------------------------------------------------
# Header
# ---------------------------------------------------------------------------
Write-Header 'UnrealMCP Audio Tools Installer'
Write-Info "Plugin root : $PluginRoot"
Write-Info "Third party : $ThirdParty"
Write-Info "Audio server: $AudioServer"
Write-Host ''

# ---------------------------------------------------------------------------
# Prerequisite checks
# ---------------------------------------------------------------------------
Write-Header 'Checking prerequisites'

$errors   = [System.Collections.Generic.List[string]]::new()
$warnings = [System.Collections.Generic.List[string]]::new()

# --- Python 3.10 ---
$pythonCmd = $null
if (Test-Command 'python') {
    $ver = & python --version 2>&1
    if ($ver -match '3\.10\.\d+') {
        $pythonCmd = 'python'
        Write-Ok "  Python 3.10 found: $ver"
    } else {
        Write-Warn "  Default Python is $ver; looking for Python 3.10 via launcher..."
    }
}
if (-not $pythonCmd -and (Test-Command 'py')) {
    $pyVer = & py -3.10 --version 2>&1
    if ($LASTEXITCODE -eq 0 -and $pyVer -match '3\.10\.\d+') {
        $pythonCmd = 'py -3.10'
        Write-Ok "  Python launcher 3.10 found: $pyVer"
    }
}
if (-not $pythonCmd) {
    $errors.Add('Python 3.10 is required. Install from https://www.python.org/downloads/release/python-31011/ or use conda.')
    Write-Err '  Python 3.10 not found.'
}

# --- Git ---
if (Test-Command 'git') {
    $gitVer = & git --version 2>&1
    Write-Ok "  Git found: $gitVer"
} else {
    $errors.Add('Git is required to clone repositories. Install from https://git-scm.com/download/win')
    Write-Err '  Git not found.'
}

# --- Git LFS (recommended) ---
if (Test-Command 'git') {
    $lfsVer = & git lfs version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "  Git LFS found: $lfsVer"
    } else {
        $warnings.Add('Git LFS is recommended for large model files. Install from https://git-lfs.com/')
        Write-Warn '  Git LFS not detected.'
    }
}

# --- FFmpeg (recommended) ---
if (Test-Command 'ffmpeg') {
    $ffVer = (& ffmpeg -version 2>&1 | Select-Object -First 1)
    Write-Ok "  FFmpeg found: $ffVer"
} else {
    $warnings.Add('FFmpeg is required by several audio tools. Install via conda or from https://ffmpeg.org/download.html')
    Write-Warn '  FFmpeg not found.'
}

# --- conda / python ---
$hasConda = Test-Command 'conda'
if ($hasConda) {
    $condaVer = & conda --version 2>&1
    Write-Ok "  Conda found: $condaVer"
} else {
    Write-Warn '  Conda not found; will fall back to Python venv.'
    if (-not $pythonCmd) {
        $errors.Add('Neither conda nor Python 3.10 is available. Cannot create environment.')
    }
}

# --- NVIDIA GPU / CUDA runtime check ---
$hasNvidia = $false
if (Test-Command 'nvidia-smi') {
    $smi = & nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>&1
    if ($LASTEXITCODE -eq 0) {
        $hasNvidia = $true
        Write-Ok "  NVIDIA GPU detected: $smi"
    } else {
        Write-Warn '  nvidia-smi present but failed to query GPU.'
    }
} else {
    Write-Warn '  nvidia-smi not found; GPU support unavailable.'
}

# Stop if critical prerequisites are missing
if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Err 'Missing required prerequisites. Please resolve the errors below and re-run:'
    $errors | ForEach-Object { Write-Err "  - $_" }
    if ($warnings.Count -gt 0) {
        Write-Warn 'Also note:'
        $warnings | ForEach-Object { Write-Warn "  - $_" }
    }
    exit 1
}

# ---------------------------------------------------------------------------
# Create environment
# ---------------------------------------------------------------------------
Write-Header 'Creating Python environment'

[string]$envType    = $null
[string]$activateCmd = $null

if ($hasConda) {
    $envType = 'conda'
    $activateCmd = 'conda activate ai-audio'

    $envExists = (& conda env list 2>&1 | Select-String -Pattern '^ai-audio\s')
    if ($envExists) {
        Write-Warn "  Conda environment 'ai-audio' already exists; skipping creation."
    } else {
        if ($PSCmdlet.ShouldProcess('ai-audio', 'conda create')) {
            Invoke-Step { & conda create -n ai-audio python=3.10 -y } 'Creating conda environment ai-audio'
        }
    }

    # Use conda run so we do not depend on shell activation working in PowerShell.
    Write-Ok "  Using conda environment 'ai-audio'."
} else {
    if (-not $pythonCmd) {
        Write-Err 'No Python 3.10 command available and conda not found. Aborting.'
        exit 1
    }

    $envType = 'venv'
    $activateCmd = "& '$VenvPath\Scripts\Activate.ps1'"

    if (Test-Path $VenvPath) {
        if ($Force) {
            Write-Warn "  Removing existing venv at $VenvPath (Force specified)."
            Remove-Item -Recurse -Force $VenvPath
        } else {
            Write-Warn "  venv already exists at $VenvPath; skipping creation."
        }
    }

    if (-not (Test-Path $VenvPath)) {
        if ($PSCmdlet.ShouldProcess($VenvPath, 'python -m venv')) {
            # $pythonCmd may be 'py -3.10' (with a space). Split into executable
            # and arguments so the call operator finds the real command.
            $pythonParts = $pythonCmd -split ' '
            $pythonExe   = $pythonParts[0]
            $pythonArgs  = $pythonParts[1..($pythonParts.Length - 1)]
            Invoke-Step { & $pythonExe @pythonArgs -m venv $VenvPath } "Creating venv at $VenvPath"
        }
    }

    Write-Ok "  Using venv at $VenvPath."
}

# Helper to run a pip command inside the selected environment
function Invoke-EnvPip {
    [CmdletBinding(SupportsShouldProcess = $true)]
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments,
        [Parameter(Mandatory)]
        [string]$Description,
        [switch]$IgnoreExitCode
    )
    if (-not $PSCmdlet.ShouldProcess(($Arguments -join ' '), $Description)) {
        return
    }
    Write-Info "  > $Description"
    Write-Info "     pip $($Arguments -join ' ')"
    if ($envType -eq 'conda') {
        & conda run -n ai-audio --no-capture-output pip @Arguments
    } else {
        & "$VenvPath\Scripts\python.exe" -m pip @Arguments
    }
    if (-not $IgnoreExitCode -and $LASTEXITCODE -ne 0) {
        throw "pip step failed: $Description (exit code $LASTEXITCODE)"
    }
}

# Upgrade pip/setuptools/wheel
Invoke-EnvPip @('install', '--upgrade', 'pip', 'setuptools', 'wheel') 'Upgrading packaging tools'

# ---------------------------------------------------------------------------
# Install PyTorch
# ---------------------------------------------------------------------------
Write-Header 'Installing PyTorch'

if ($hasNvidia) {
    Write-Info "  Attempting CUDA 12.6 wheel ($CudaIndexUrl)."
    try {
        Invoke-EnvPip @('install', 'torch', 'torchvision', 'torchaudio', '--index-url', $CudaIndexUrl) 'Installing PyTorch with CUDA 12.6'
        Write-Ok '  PyTorch (CUDA) installed successfully.'
    } catch {
        Write-Warn "  CUDA install failed: $_"
        Write-Warn '  Falling back to CPU-only PyTorch.'
        Invoke-EnvPip @('install', 'torch', 'torchvision', 'torchaudio', '--index-url', 'https://download.pytorch.org/whl/cpu') 'Installing PyTorch CPU-only'
    }
} else {
    Write-Warn '  No NVIDIA GPU detected; installing CPU-only PyTorch.'
    Invoke-EnvPip @('install', 'torch', 'torchvision', 'torchaudio', '--index-url', 'https://download.pytorch.org/whl/cpu') 'Installing PyTorch CPU-only'
}

# Optional: triton-windows for ACE-Step torch.compile on Windows + CUDA
if ($envType -eq 'conda' -and $hasNvidia) {
    Invoke-EnvPip @('install', 'triton-windows') 'Installing triton-windows (optional, for torch.compile)' -IgnoreExitCode
}

# ---------------------------------------------------------------------------
# Clone and install audio repositories
# ---------------------------------------------------------------------------
if (-not $SkipRepoClone) {
    Write-Header 'Cloning audio model repositories'
    New-Item -ItemType Directory -Force -Path $ThirdParty | Out-Null

    foreach ($repo in $Repos) {
        $dest = Join-Path $ThirdParty $repo.DirName
        Write-Info "  Installing $($repo.Name)..."

        if (Test-Path $dest) {
            if ($Force) {
                Write-Warn "    Removing existing $($repo.DirName) (Force specified)."
                Remove-Item -Recurse -Force $dest
            } else {
                Write-Warn "    $($repo.DirName) already exists; skipping clone. Use -Force to re-clone."
            }
        }

        if (-not (Test-Path $dest)) {
            if ($PSCmdlet.ShouldProcess($repo.Url, 'git clone')) {
                try {
                    Invoke-Step { & git clone $repo.Url $dest } "Cloning $($repo.Name)"
                } catch {
                    Write-Err "    Failed to clone $($repo.Name): $_"
                    Write-Warn '    Skipping install for this repository. Fix network access and re-run.'
                    continue
                }
            }
        }

        if (-not (Test-Path $dest)) {
            Write-Warn "    Directory $dest does not exist; skipping install for $($repo.Name)."
            continue
        }

        # Run install command
        $installOk = $false
        Push-Location $dest
        try {
            try {
                Invoke-EnvPip $repo.InstallArgs "Installing $($repo.Name) in editable mode"
                $installOk = $true
            } catch {
                if ($repo.FallbackArgs) {
                    Write-Warn "    Primary install failed; trying fallback: pip $($repo.FallbackArgs -join ' ')"
                    Invoke-EnvPip $repo.FallbackArgs "Installing $($repo.Name) (fallback)"
                    $installOk = $true
                } else {
                    throw
                }
            }
        } finally {
            Pop-Location
        }

        if ($installOk) {
            Write-Ok "    $($repo.Name) installed."
        }
    }
} else {
    Write-Warn '  -SkipRepoClone specified; skipping repository clone/install.'
}

# ---------------------------------------------------------------------------
# Guard against model packages downgrading PyTorch to a CPU-only build
# ---------------------------------------------------------------------------
Write-Header 'Verifying PyTorch CUDA build'

if ($hasNvidia) {
    $cudaOk = $false
    try {
        if ($envType -eq 'conda') {
            $torchVer = (& conda run -n ai-audio --no-capture-output python -c "import torch; print(torch.__version__)") | Select-Object -Last 1
            $cudaOk   = (& conda run -n ai-audio --no-capture-output python -c "import torch; print(torch.cuda.is_available())") | Select-Object -Last 1
        } else {
            $torchVer = (& "$VenvPath\Scripts\python.exe" -c "import torch; print(torch.__version__)")
            $cudaOk   = (& "$VenvPath\Scripts\python.exe" -c "import torch; print(torch.cuda.is_available())")
        }
        Write-Info "  Current torch: $torchVer ; CUDA available: $cudaOk"
    } catch {
        Write-Warn "  Could not query torch state: $_"
    }

    if (-not ($cudaOk -eq 'True')) {
        Write-Warn '  CUDA torch not active; force-reinstalling CUDA 12.6 wheels.'
        Invoke-EnvPip @('install', '--force-reinstall', 'torch', 'torchvision', 'torchaudio', '--index-url', $CudaIndexUrl) 'Re-installing PyTorch CUDA 12.6 wheels'
    } else {
        Write-Ok '  CUDA torch is active.'
    }
} else {
    Write-Warn '  No NVIDIA GPU; skipping CUDA verification.'
}

# ---------------------------------------------------------------------------
# Create audio_server directories and .env
# ---------------------------------------------------------------------------
Write-Header 'Configuring audio_server'

New-Item -ItemType Directory -Force -Path $AudioServer | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Write-Ok "  Output directory: $OutputDir"

# Build .env content
$deviceSetting = if ($hasNvidia) { 'cuda' } else { 'cpu' }
$envPath = Join-Path $AudioServer '.env'

$aceDir    = (Join-Path $ThirdParty 'ACE-Step').Replace('\', '/')
$saDir     = (Join-Path $ThirdParty 'stable-audio-tools').Replace('\', '/')
$mmaDir    = (Join-Path $ThirdParty 'MMAudio').Replace('\', '/')
$outDirFwd = $OutputDir.Replace('\', '/')

$envContent = @"
# UnrealMCP audio-designer server configuration
# Generated by Plugins/UnrealMCP/scripts/install-audio-tools.ps1

# Paths to installed model repositories
ACE_STEP_DIR=$aceDir
STABLE_AUDIO_DIR=$saDir
MMAUDIO_DIR=$mmaDir

# Audio server settings (read by audio_server/main.py when loaded into the environment)
AUDIO_SERVER_HOST=127.0.0.1
AUDIO_SERVER_PORT=8123
AUDIO_SERVER_DEVICE=$deviceSetting
AUDIO_SERVER_OUTPUT_DIR=$outDirFwd

# Backwards-compatible aliases
DEVICE=$deviceSetting
OUTPUT_DIR=$outDirFwd

# Default Gradio ports for each tool
ACE_STEP_PORT=7865
STABLE_AUDIO_PORT=7860
MMAUDIO_PORT=7860

# HuggingFace cache (optional; leave blank to use default)
# HF_HOME=
"@

if ($PSCmdlet.ShouldProcess($envPath, 'Write .env')) {
    Set-Content -Path $envPath -Value $envContent -Encoding UTF8 -Force
    Write-Ok "  Wrote $envPath"
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
Write-Header 'Installation summary'

if ($envType -eq 'conda') {
    Write-Ok '  Environment: conda env "ai-audio"'
    Write-Info '  Activate:    conda activate ai-audio'
} else {
    Write-Ok "  Environment: venv at $VenvPath"
    Write-Info "  Activate:    & '$VenvPath\Scripts\Activate.ps1'"
}

Write-Info "  Third-party code: $ThirdParty"
Write-Info "  Output directory: $OutputDir"
Write-Info "  Config file:      $envPath"
Write-Host ''

Write-Header 'Verification commands (run one at a time)'
foreach ($repo in $Repos) {
    $dest = Join-Path $ThirdParty $repo.DirName
    if (Test-Path $dest) {
        Write-Info "  $($repo.Name): $dest"
        Write-Host "    cd $dest" -ForegroundColor DarkGray
        Write-Host "    $($repo.VerifyCmd)" -ForegroundColor DarkGray
    } else {
        Write-Warn "  $($repo.Name): not cloned (see errors above)."
    }
}

Write-Host ''
Write-Warn 'Notes:'
Write-Warn '  - Stable Audio Open weights require a HuggingFace account + accepting the model terms.'
Write-Warn '  - First launch of each model downloads checkpoints; this can take several minutes.'
Write-Warn '  - If you see CUDA out-of-memory errors, use --cpu_offload or switch DEVICE=cpu in .env.'
Write-Host ''
Write-Ok 'Done.'
