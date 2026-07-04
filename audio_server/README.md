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
- FFmpeg
- CUDA 11.8+ (optional but strongly recommended)

Install the three model packages first by following the official guides referenced in `../docs/audio-tools-installation.md`.

## Setup

```powershell
# Optional: create a dedicated virtual environment
python -m venv venv
.\venv\Scripts\Activate.ps1

# Install server dependencies
pip install -r requirements.txt

# Install model packages (see requirements.txt comments for exact commands)
```

Edit `config.yaml` to set model checkpoint paths, device, port, and output directory.

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
- The exact function names inside the model wrappers are marked with `TODO` comments; verify them against the installed versions of ACE-Step, stable-audio-tools, and MMAudio.
