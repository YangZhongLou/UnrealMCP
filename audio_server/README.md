# UnrealMCP Audio Generation Server

A local FastAPI service that wraps three open-source audio generation models so Unreal Engine can generate music, SFX, and Foley at runtime via HTTP.

| Endpoint | Model | Use case |
| --- | --- | --- |
| `POST /generate/music` | ACE-Step | BGM / theme songs / full songs |
| `POST /generate/sfx` | Stable Audio Open | Sound effects / ambience / short loops |
| `POST /generate/foley` | MMAudio | Video-to-audio or text-to-audio Foley |

## Prerequisites

- Python 3.10
- Git and Git LFS
- FFmpeg (Windows: BtbN **shared** build; `avcodec-*.dll`, `avformat-*.dll`, `avutil-*.dll` must be on `PATH`)
- CUDA 11.8+ (optional but strongly recommended)

Install the three model packages first by following the official guides referenced in `../docs/audio-tools-installation.md`.

## Setup

The recommended way is to use the provided installer, which creates a venv, installs PyTorch with CUDA, clones the three model repositories, and writes `audio_server/.env`:

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

If you prefer a manual setup:

```powershell
# Optional: create a dedicated virtual environment
python -m venv venv
.\venv\Scripts\Activate.ps1

# Install server dependencies
pip install -r requirements.txt

# Install model packages (see requirements.txt comments for exact commands)
```

Edit `config.yaml` (or set environment variables in `audio_server/.env`) to set model checkpoint paths, device, port, and output directory.

## Pre-downloaded weights / offline migration

If downloading weights from inside this environment is unreliable, pre-download them on a machine with better network access and copy them into the project with the import helper.

The current default MMAudio variant is **`medium_44k`**. For a fully offline `/generate/foley` call you also need the CLIP and BigVGAN files that MMAudio normally downloads from HuggingFace at runtime.

### Expected source layout

```text
D:/AudioWeights/
├── ACE-Step/
│   └── checkpoints/
│       ├── music_dcae_f8c8/
│       ├── music_vocoder/
│       ├── ace_step_transformer/
│       └── umt5-base/
├── stable-audio-open/
│   └── models--stabilityai--stable-audio-open-1.0/   # HuggingFace hub snapshot
└── MMAudio/
    ├── weights/
    │   ├── mmaudio_medium_44k.pth        # or mmaudio_large_44k_v2.pth
    │   └── open_clip_pytorch_model.bin   # CLIP, auto-detected by main.py
    ├── ext_weights/
    │   ├── v1-44.pth
    │   ├── synchformer_state_dict.pth
    │   └── nvidia/bigvgan_v2_44khz_128band_512x/
    │       ├── config.json
    │       └── bigvgan_generator.pt      # BigVGAN, auto-detected by main.py
    └── ...
```

### Import

From the project root (`D:/Playground/TA-Playground`):

```powershell
.\Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -UpdateEnv
```

The script will:

- Validate the source tree for each model and report missing files.
- Copy ACE-Step checkpoints to `Plugins/UnrealMCP/third_party/ACE-Step/checkpoints/` and update `ACE_STEP_CHECKPOINT_PATH` in `audio_server/.env` (when `-UpdateEnv` is used).
- Copy the Stable Audio Open HuggingFace cache snapshot to `%HF_HOME%\hub\models--stabilityai--stable-audio-open-1.0` (or `TRANSFORMERS_CACHE\hub\...`).
- Copy MMAudio weights to both `third_party/MMAudio/` and `audio_server/` so the relative `./weights` and `./ext_weights` paths resolve when the server is launched from `audio_server/`.
- Verify any `.md5` sidecar files and report checksum mismatches.

Preview changes without touching anything:

```powershell
.\Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -WhatIf
```

Create directory junctions instead of copying (saves disk space, but requires the source to remain available):

```powershell
.\Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -Symlink -UpdateEnv
```

Skip individual models with `-SkipAceStep`, `-SkipStableAudio`, or `-SkipMMAudio`.

## Start the server

```powershell
python main.py
```

By default the server listens on `http://127.0.0.1:8123`.

## Example requests

### Health check

```powershell
curl -X POST http://127.0.0.1:8123/health
```

### Generate music

```powershell
curl -X POST http://127.0.0.1:8123/generate/music `
  -H "Content-Type: application/json" `
  -d '{"prompt":"cinematic orchestral, tense, strings and brass, 110 BPM, instrumental","duration_seconds":30}'
```

### Generate SFX

```powershell
curl -X POST http://127.0.0.1:8123/generate/sfx `
  -H "Content-Type: application/json" `
  -d '{"prompt":"heavy footsteps on wet concrete, close-miked","duration_seconds":4}'
```

### Generate Foley from text

```powershell
curl -X POST http://127.0.0.1:8123/generate/foley `
  -H "Content-Type: application/json" `
  -d '{"prompt":"coffee shop ambiance with gentle chatter and espresso machine","duration_seconds":8}'
```

### Generate Foley for a video

```powershell
curl -X POST http://127.0.0.1:8123/generate/foley `
  -H "Content-Type: application/json" `
  -d '{"video_path":"D:/videos/gameplay.mp4","prompt":"footsteps on gravel and cloth rustling","duration_seconds":8}'
```

## Unreal Engine HTTP example

Use the `Construct JSON Object` and `HTTP POST` nodes to call the endpoints. Parse the returned JSON and use the `audio_base64` field with a plugin such as **Runtime Audio Importer** to create a `USoundWave` at runtime.

## Output

Generated files are saved as 16-bit PCM WAV files under `audio_server/output/<model>/`. The response includes both the local `file_path` and `audio_base64` bytes.

## Notes

- Models are loaded lazily on the first request to keep startup fast.
- First request for each model downloads weights from HuggingFace / the model's own CDN **unless** local weights are detected.
- For MMAudio, place `open_clip_pytorch_model.bin` and the `nvidia_bigvgan_v2_44khz_128band_512x/` folder under the project-root `weights/` directory. `main.py` will set `MMAUDIO_CLIP_PATH` and `BIGVGAN_LOCAL_DIR` automatically and skip the HuggingFace download.
- If your connection to HuggingFace is slow but usable, use the resume-download scripts in `weights/_download_clip.py` and `weights/_download_bigvgan.py`.
- Stable Audio Open requires a HuggingFace account and accepting the model terms.

## Windows notes / known issues

1. **FFmpeg must be the shared BtbN Windows build.** Static `ffmpeg.exe` is not enough because `torchcodec` loads `avcodec-*.dll`, `avformat-*.dll`, `avutil-*.dll` at runtime. Download `ffmpeg-master-latest-win64-gpl-shared.zip` from [BtbN FFmpeg Builds](https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip), extract so `tools/ffmpeg/bin/` contains the DLLs, and start the server with that directory on `PATH`. Missing FFmpeg manifests as errors about `libtorchcodec_core*.dll` / FFmpeg not found when calling `/generate/music`.

2. **ACE-Step needs local patches on Windows** with `accelerate` 1.6 + `transformers` 4.50:
   - Pass `low_cpu_mem_usage=False` to `from_pretrained` in `acestep/pipeline_ace_step.py` (transformer + text encoder) and `acestep/music_dcae/music_dcae_pipeline.py` (DCAE + vocoder). Without this, loading raises `Cannot copy out of meta tensor; no data!`.
   - Replace `torchaudio.save(..., backend="soundfile")` with `soundfile.write` in `save_wav_file`, because recent `torchaudio` ignores the backend arg and routes through `torchcodec` (see the FFmpeg note above).
   - Patch file: `Plugins/UnrealMCP/scripts/patches/ace-step-low-cpu-mem-and-soundfile.patch`.

3. **Verified generation times on this hardware:** 30 s music ≈ 14 s; 180 s (3-minute) music ≈ 143 s.
