# UnrealMCP Audio Tools Installer

This folder contains a one-click PowerShell installer for the external AI audio models used by the UnrealMCP `audio-designer` agent.

## What it installs

| Tool | Repository | Purpose |
| --- | --- | --- |
| ACE-Step | <https://github.com/ace-step/ACE-Step> | Music / BGM / theme generation |
| Stable Audio Open | <https://github.com/Stability-AI/stable-audio-tools> | SFX / ambient / short-loop generation |
| MMAudio | <https://github.com/hkchengrex/MMAudio> | Video / text-to-audio Foley |

The script will:

1. Check prerequisites (Python 3.10, Git, Git LFS, FFmpeg, conda or python).
2. Create a conda environment named `ai-audio` with Python 3.10 (preferred), or fall back to a venv at `Plugins/UnrealMCP/.venv`.
3. Install PyTorch with CUDA 12.4 support, falling back to CPU-only if no NVIDIA GPU is detected.
4. Clone the three repositories into `Plugins/UnrealMCP/third_party/` and install each in editable mode.
5. Create `Plugins/UnrealMCP/audio_server/output/` for generated audio.
6. Write `Plugins/UnrealMCP/audio_server/.env` with default paths and device settings.

## Prerequisites

- Windows PowerShell 5.1 or later (PowerShell 7 also works).
- [Python 3.10](https://www.python.org/downloads/release/python-31011/)
- [Git](https://git-scm.com/download/win)
- [Git LFS](https://git-lfs.com/) (recommended for large checkpoints)
- [FFmpeg](https://ffmpeg.org/download.html)
- [conda / miniforge](https://github.com/conda-forge/miniforge) (optional but preferred)
- NVIDIA GPU + recent driver (optional; CPU fallback is supported but slow)

## How to run

Open PowerShell in the project root (`D:/Playground/TA-Playground`) and run:

```powershell
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

### Execution policy

PowerShell scripts are blocked by default. If you see an error about execution policy, use one of these options:

**Option A — scope the policy to the current process (safest):**

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process -Force
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

**Option B — scope it to the current user:**

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

### Command-line options

```powershell
# Re-clone repositories even if they already exist
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1 -Force

# Preview changes without installing anything
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1 -WhatIf

# Skip cloning / repository install (useful to re-run just the environment setup)
.\Plugins\UnrealMCP\scripts\install-audio-tools.ps1 -SkipRepoClone
```

## Verification

After installation, activate the environment and verify each model:

```powershell
# conda
conda activate ai-audio

# or venv
& .\Plugins\UnrealMCP\.venv\Scripts\Activate.ps1
```

ACE-Step:

```powershell
cd Plugins/UnrealMCP/third_party/ACE-Step
acestep --port 7865
```

Stable Audio Open:

```powershell
cd Plugins/UnrealMCP/third_party/stable-audio-tools
python run_gradio.py --pretrained-name stabilityai/stable-audio-open-1.0
```

MMAudio:

```powershell
cd Plugins/UnrealMCP/third_party/MMAudio
python demo.py --prompt "coffee shop ambiance" --duration 8
```

## Troubleshooting

| Problem | Likely cause | Fix |
| --- | --- | --- |
| `cannot be loaded because running scripts is disabled` | PowerShell execution policy | Run `Set-ExecutionPolicy RemoteSigned -Scope Process -Force` first. |
| `Python 3.10 not found` | Default `python` is another version | Install Python 3.10 or use conda. |
| `conda not found` | conda not on PATH | Either install conda/miniforge or ensure Python 3.10 is available so the script falls back to venv. |
| CUDA out of memory | GPU VRAM too small | Edit `Plugins/UnrealMCP/audio_server/.env` and set `DEVICE=cpu`, or use model-specific offloading flags. |
| Stable Audio Open download fails | HuggingFace terms not accepted | Run `huggingface-cli login`, then accept the terms at [stabilityai/stable-audio-open-1.0](https://huggingface.co/stabilityai/stable-audio-open-1.0). |
| MMAudio install fails with `setup.py not found` | Old pip version | The script upgrades pip first; if it still fails, manually run `pip install --upgrade pip` in the environment. |
| Clone fails / network timeout | Firewall / GitHub access | Re-run the script after checking connectivity. Use `-Force` to re-clone a partially downloaded repo. |

## Files created by the script

- `Plugins/UnrealMCP/third_party/ACE-Step`
- `Plugins/UnrealMCP/third_party/stable-audio-tools`
- `Plugins/UnrealMCP/third_party/MMAudio`
- `Plugins/UnrealMCP/audio_server/output/`
- `Plugins/UnrealMCP/audio_server/.env`
- `Plugins/UnrealMCP/.venv/` (if conda is not used)

## See also

- Detailed manual guide: `Plugins/UnrealMCP/docs/audio-tools-installation.md`
- Agent definition: `SkillHub/.claude/agents/audio-designer.md`
