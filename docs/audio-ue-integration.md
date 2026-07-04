# Unreal Engine Audio Generation Integration Guide

This guide shows how to call the local UnrealMCP audio generation server from Unreal Engine. The server exposes three HTTP endpoints that return generated audio as base64-encoded WAV data.

| Endpoint | Model | Use case |
| --- | --- | --- |
| `POST /generate/music` | ACE-Step | BGM / theme songs / full songs |
| `POST /generate/sfx` | Stable Audio Open | Sound effects / ambience / short loops |
| `POST /generate/foley` | MMAudio | Video-to-audio or text-to-audio Foley |

## Architecture overview

```text
Unreal Engine
├── Blueprint HTTP nodes  ──┐
└── Python Editor Scripting ─┤
                             │  HTTP POST (JSON)
                             ▼
               http://127.0.0.1:8123
                        │
                        ▼
            FastAPI audio_server (main.py)
                        │
            ┌───────────┼───────────┐
            ▼           ▼           ▼
         ACE-Step   Stable Audio   MMAudio
                        │
                        ▼
              WAV file + audio_base64
                        │
            ┌───────────┴───────────┐
            ▼                       ▼
    Import as USoundWave asset    Play at runtime
    (/Game/GeneratedAudio)        (Runtime Audio Importer)
```

The server runs outside Unreal Engine as a regular Python process. Unreal sends a JSON request, the server runs the selected model, and returns metadata plus the WAV bytes encoded as base64. From there you can either save the WAV to disk and import it as a `USoundWave` asset, or decode and play it at runtime.

## Start the audio server

There is no dedicated `start-audio-server.ps1` script in this repository yet. Start the server manually from the `audio_server` directory:

```powershell
cd "D:/Playground/TA-Playground/Plugins/UnrealMCP/audio_server"
python -m uvicorn audio_server.main:app --host 127.0.0.1 --port 8123
```

Or, if the server package was installed with the project layout:

```powershell
cd "D:/Playground/TA-Playground/Plugins/UnrealMCP/audio_server"
python main.py
```

By default the server listens on `http://127.0.0.1:8123`. Verify it is alive with:

```powershell
curl -X POST http://127.0.0.1:8123/health
```

## Blueprint HTTP integration

This workflow uses the Unreal Engine **HTTP** plugin (enabled by default in most projects) and standard Blueprint JSON nodes. It is suitable for Editor Utility Widgets, editor automation, or runtime generation if you also decode the base64 response.

### 1. Build the request body

Use `Make Json Object` and add string fields for the parameters. Example for `/generate/sfx`:

| Field | Type | Example |
| --- | --- | --- |
| `prompt` | String | `"heavy footsteps on wet concrete, close-miked"` |
| `duration_seconds` | Number | `4` |

For `/generate/foley` you can optionally add a `video_path` string.

### 2. Send the POST request

- Drag out from the JSON object → `Construct JSON Request`.
- `Set Header` on the request object: `Content-Type` = `application/json`.
- `Set URL` to `http://127.0.0.1:8123/generate/sfx` (or `music` / `foley`).
- `Process URL` → bind to `On Request Complete`.

### 3. Parse the response

The response body is a JSON object. Key fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `success` | Boolean | Whether generation succeeded |
| `audio_base64` | String | WAV bytes, base64 encoded |
| `file_path` | String | Local path on the server (useful for debugging) |
| `error` | String | Error message when `success` is false |

Use `Get Object Field` to get the root object, then `Get String Field` for `audio_base64`.

### 4. Save the WAV file

To create a persistent asset, write the base64 string to a `.wav` file under `Saved/GeneratedAudio/`:

**Option A: Blueprint File Library**

- `Get Project Saved Directory` → `Append` `/GeneratedAudio/MySfx.wav`.
- Convert the base64 string back to bytes. Blueprint has no built-in base64 decoder, so use a plugin such as **Runtime Audio Importer** or call a small Python helper.
- `Save String to File` can only store text, so this path works best when combined with a Python Editor Scripting step.

**Option B: Python Editor Scripting helper**

Create a Python asset action or Editor Utility that:

1. Receives the base64 string from Blueprint (via `Execute Python Script` or an exposed function).
2. Decodes it with `base64.b64decode`.
3. Writes it to `Saved/GeneratedAudio/<timestamp>.wav`.
4. Calls `unreal.AssetToolsHelpers.get_asset_tools().create_asset` with `unreal.SoundWave` as the asset class and the WAV import factory.

See the next section for a complete Python example.

### 5. Import as USoundWave asset

Once the WAV is saved on disk, import it into the Content Browser:

- In an **Editor Utility Widget** or **Editor Utility Blueprint**, use the `Import File as Texture/ Sound...` node family or call the equivalent Python import command.
- Target package path: `/Game/GeneratedAudio/MySfx`.

Alternatively, use the **Runtime Audio Importer** plugin to skip disk import and play the decoded PCM data directly at runtime. Note that the open-source version of Runtime Audio Importer has been archived; the maintained version is available on Fab.

## Python Editor Scripting integration

A simpler, all-in-one approach is to run a Python script inside Unreal Engine that calls the endpoint and creates the `SoundWave` asset for you.

```python
import unreal
import requests
import base64
import os

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
ENDPOINT = "http://127.0.0.1:8123/generate/sfx"
REQUEST_BODY = {
    "prompt": "heavy footsteps on wet concrete, close-miked",
    "duration_seconds": 4,
}

CONTENT_DIR = unreal.Paths.project_content_dir()
PACKAGE_DIR = "/Game/GeneratedAudio"
LOCAL_DIR = os.path.join(CONTENT_DIR, "GeneratedAudio")
os.makedirs(LOCAL_DIR, exist_ok=True)

ASSET_NAME = "GenSfx_01"
LOCAL_WAV = os.path.join(LOCAL_DIR, f"{ASSET_NAME}.wav")
PACKAGE_PATH = f"{PACKAGE_DIR}/{ASSET_NAME}"

# ---------------------------------------------------------------------------
# Call the local audio server
# ---------------------------------------------------------------------------
response = requests.post(ENDPOINT, json=REQUEST_BODY, timeout=120)
response.raise_for_status()
result = response.json()

if not result.get("success"):
    raise RuntimeError(f"Server reported failure: {result.get('error')}")

audio_base64 = result["audio_base64"]
wav_bytes = base64.b64decode(audio_base64)

with open(LOCAL_WAV, "wb") as f:
    f.write(wav_bytes)

unreal.log(f"Saved generated WAV to {LOCAL_WAV}")

# ---------------------------------------------------------------------------
# Import the WAV as a USoundWave asset
# ---------------------------------------------------------------------------
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

# Optional: configure import settings via SoundFactoryCreateFileSettings
factory = unreal.SoundFactory()
factory.suppress_editor_dialogs = True

imported = asset_tools.create_asset(
    asset_name=ASSET_NAME,
    package_path=PACKAGE_DIR,
    asset_class=unreal.SoundWave,
    factory=factory,
)

if imported is None:
    # Fallback: use automated import if create_asset is not sufficient
    task = unreal.AssetImportTask()
    task.filename = LOCAL_WAV
    task.destination_path = PACKAGE_DIR
    task.destination_name = ASSET_NAME
    task.replace_existing = True
    task.automated = True
    task.save = True

    factory = unreal.SoundFactory()
    factory.suppress_editor_dialogs = True
    task.factory = factory

    asset_tools.import_asset_tasks([task])
    imported = unreal.EditorAssetLibrary.load_asset(PACKAGE_PATH)

unreal.log(f"Imported SoundWave asset: {PACKAGE_PATH}")
```

To run this script inside Unreal Editor:

1. Enable the **Python Editor Scripting Plugin** in your project.
2. Open **Window → Developer Tools → Python** and paste/run the script, or save it to the project's `Content/Python/` folder and execute it from an Editor Utility Widget.

## Model weights and offline use

The three models are loaded lazily on first request. Their weights are large and are usually downloaded from HuggingFace or the model's own CDN the first time an endpoint is used. If your network is slow or unreliable:

- Pre-download the weights on a machine with better connectivity.
- Follow the import instructions in [`../audio_server/MODEL_DOWNLOAD_GUIDE.md`](../audio_server/MODEL_DOWNLOAD_GUIDE.md).
- Use the helper script to copy them into the project:

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -UpdateEnv
```

For a dry run that shows what would change:

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -WhatIf
```

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `Connection refused` / request fails | Audio server is not running | Start `main.py` or `uvicorn` from `Plugins/UnrealMCP/audio_server` and verify with `curl http://127.0.0.1:8123/health` |
| HTTP 500, model not loaded | Missing weights or CUDA out of memory | Check the server console for the exact traceback; pre-download weights via `import-audio-weights.ps1`; reduce `duration_seconds` or enable CPU offload in `config.yaml` |
| First request is very slow | Lazy model loading + weight download | This is expected; subsequent requests reuse the loaded model. Pre-download weights and avoid reusing short editor sessions |
| `audio_base64` cannot be decoded | The string was truncated or stored as plain text by Blueprint | Decode it with Python (`base64.b64decode`) and write bytes, not characters, to the `.wav` file |
| Imported WAV is silent or distorted | Wrong import factory or corrupted bytes | Verify the saved file plays in an external media player before importing; ensure you write binary bytes, not the base64 string |
| `Import failed` from Python | Package path or factory mismatch | Use `/Game/GeneratedAudio` (not disk path) as `package_path`; confirm `SoundFactory` is the correct factory class |

For model-specific setup issues, also see [`audio-tools-installation.md`](audio-tools-installation.md) and the server README at [`../audio_server/README.md`](../audio_server/README.md).
