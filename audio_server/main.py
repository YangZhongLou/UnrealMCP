"""Local FastAPI service that wraps ACE-Step, Stable Audio Open, and MMAudio.

Intended to be called from Unreal Engine via HTTP to generate music, SFX, and
Foley at runtime. Models are loaded lazily on first request to keep startup fast
and avoid loading models that are not installed.
"""

from __future__ import annotations

import base64
import logging
import os
import sys
import time
import traceback
from datetime import datetime
from http import HTTPStatus
from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf
import torch
import yaml
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

# ---------------------------------------------------------------------------
# Optional .env loading and editable package paths
# ---------------------------------------------------------------------------

try:
    from dotenv import load_dotenv
except ImportError:  # python-dotenv not installed; env vars must come from the parent environment
    load_dotenv = None  # type: ignore[assignment,misc]

if load_dotenv is not None:
    env_path = Path(__file__).with_name(".env")
    if env_path.exists():
        load_dotenv(dotenv_path=env_path, override=False, verbose=False)

for _env_name, _dir_path in (
    ("ACE_STEP_DIR", os.getenv("ACE_STEP_DIR")),
    ("STABLE_AUDIO_DIR", os.getenv("STABLE_AUDIO_DIR")),
    ("MMAUDIO_DIR", os.getenv("MMAUDIO_DIR")),
):
    if _dir_path:
        _resolved = Path(_dir_path).resolve()
        if _resolved.is_dir() and str(_resolved) not in sys.path:
            sys.path.insert(0, str(_resolved))

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CONFIG_PATH = Path(__file__).with_name("config.yaml")


def load_config() -> dict[str, Any]:
    """Load server configuration from config.yaml, with sensible defaults."""
    defaults = {
        "host": "127.0.0.1",
        "port": 8123,
        "device": "auto",
        "output_dir": str(Path(__file__).parent / "output"),
        "models": {
            "ace_step": {
                "checkpoint_path": "",
                "package": "ace_step",
            },
            "stable_audio_open": {
                "pretrained_name": "stabilityai/stable-audio-open-1.0",
                "package": "stable_audio_tools",
            },
            "mmaudio": {
                "model_name": "large_44k_v2",
                "package": "mmaudio",
            },
        },
    }

    if CONFIG_PATH.exists():
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                overrides = yaml.safe_load(f) or {}
            defaults.update(overrides)
        except Exception as e:  # noqa: BLE001
            logging.warning("Failed to load config.yaml: %s. Using defaults.", e)

    # Allow environment variables to override config values.
    defaults["host"] = os.getenv("AUDIO_SERVER_HOST", defaults["host"])
    defaults["port"] = int(os.getenv("AUDIO_SERVER_PORT", defaults["port"]))
    defaults["device"] = os.getenv("AUDIO_SERVER_DEVICE", defaults["device"])
    defaults["output_dir"] = os.getenv("AUDIO_SERVER_OUTPUT_DIR", defaults["output_dir"])

    # Environment overrides for model-specific settings.
    defaults["models"].setdefault("ace_step", {})["checkpoint_path"] = os.getenv(
        "ACE_STEP_CHECKPOINT_PATH",
        defaults["models"].get("ace_step", {}).get("checkpoint_path", ""),
    )
    defaults["models"].setdefault("stable_audio_open", {})["pretrained_name"] = os.getenv(
        "STABLE_AUDIO_PRETRAINED_NAME",
        defaults["models"]
        .get("stable_audio_open", {})
        .get("pretrained_name", "stabilityai/stable-audio-open-1.0"),
    )
    defaults["models"].setdefault("mmaudio", {})["model_name"] = os.getenv(
        "MMAUDIO_MODEL_NAME",
        defaults["models"].get("mmaudio", {}).get("model_name", "large_44k_v2"),
    )

    return defaults


CONFIG = load_config()
OUTPUT_DIR = Path(CONFIG["output_dir"]).resolve()
DEVICE = CONFIG["device"]
if DEVICE == "auto":
    DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger("audio_server")

# ---------------------------------------------------------------------------
# FastAPI app
# ---------------------------------------------------------------------------

app = FastAPI(title="UnrealMCP Audio Generation Service")

# ---------------------------------------------------------------------------
# Model state (lazy loading)
# ---------------------------------------------------------------------------

_model_cache: dict[str, Any] = {}
_model_errors: dict[str, str] = {}

# ---------------------------------------------------------------------------
# Error classification helpers
# ---------------------------------------------------------------------------

# Exception types that almost always mean a network or corrupted download.
_DOWNLOAD_ERROR_TYPES = (
    "IncompleteRead",
    "ChunkedEncodingError",
    "ConnectionError",
    "ConnectTimeout",
    "ReadTimeout",
    "HTTPError",
    "LocalEntryNotFoundError",
    "EntryNotFoundError",
    "RepositoryNotFoundError",
    "RevisionNotFoundError",
    "GatedRepoError",
    "SafetensorError",
    "PickleError",
)

# Substrings that identify common weight/download/checksum problems.
_DOWNLOAD_ERROR_SUBSTRINGS = (
    "incomplete read",
    "checksum",
    "sha256",
    "md5",
    "corrupt",
    "corrupted",
    "download",
    "huggingface",
    "hf.co",
    "modelscope",
    "timeout",
    "connection",
    "connect",
    "ssl",
    "certificate",
    "gated",
    "access",
    "unauthorized",
    "permission",
    "not found",
    "no such file",
    "does not exist",
    "cannot find",
    "unable to find",
    "missing",
    "ckpt",
    "checkpoint",
    ".pth",
    ".safetensors",
    "state_dict",
    "weights",
    "repo",
)


def _classify_error(model_name: str, exc: BaseException) -> str:
    """Return a concise, actionable classification string for a model failure."""
    exc_type = type(exc).__name__
    exc_msg = str(exc).lower()
    tb = traceback.format_exc().lower()

    is_download = exc_type in _DOWNLOAD_ERROR_TYPES or any(
        s in exc_msg or s in tb for s in _DOWNLOAD_ERROR_SUBSTRINGS
    )
    is_import = exc_type in ("ModuleNotFoundError", "ImportError") or "no module named" in exc_msg

    if is_import:
        return "missing python package"
    if is_download:
        return "download/network or corrupted weights"
    return "runtime error"


def _model_action_hint(model_name: str, category: str) -> str:
    """Return a short "what to do next" hint for the caller."""
    if category == "missing python package":
        return (
            "Run the installer: "
            "Plugins\\UnrealMCP\\scripts\\install-audio-tools.ps1"
        )
    return (
        "Pre-download weights and verify checksums (see "
        "Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md), then update "
        "audio_server/config.yaml or audio_server/.env with local paths."
    )


def _format_model_error(model_name: str, exc: BaseException) -> str:
    """Build a clear, actionable error message for HTTP responses and health."""
    category = _classify_error(model_name, exc)
    hint = _model_action_hint(model_name, category)
    return f"{model_name}: {category} - {exc}. Next step: {hint}"


def _now_str() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S_%f")


def _ensure_output_dir(model_name: str) -> Path:
    out = OUTPUT_DIR / model_name
    out.mkdir(parents=True, exist_ok=True)
    return out


def _save_wav(audio: np.ndarray, sample_rate: int, path: Path) -> Path:
    """Save audio as a 16-bit PCM WAV file."""
    if audio.dtype != np.float32 and audio.dtype != np.float64:
        audio = audio.astype(np.float32)
    sf.write(str(path), audio, sample_rate, subtype="PCM_16", format="WAV")
    return path


def _audio_to_base64(path: Path) -> str:
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("ascii")


# ---------------------------------------------------------------------------
# ACE-Step wrapper (music / BGM)
# ---------------------------------------------------------------------------


def _load_ace_step() -> Any:
    """Lazy-load ACE-Step and return an ACEStepPipeline instance."""
    try:
        # ACE-Step installs the Python package as `acestep`.
        # The public inference class is ACEStepPipeline from acestep.pipeline_ace_step.
        from acestep.pipeline_ace_step import ACEStepPipeline  # type: ignore

        logger.info("Loading ACE-Step on %s ...", DEVICE)
        ace_config = CONFIG["models"]["ace_step"]
        checkpoint = ace_config.get("checkpoint_path") or None

        if checkpoint is not None and not Path(checkpoint).exists():
            raise FileNotFoundError(
                f"ACE-Step checkpoint path does not exist: {checkpoint}"
            )

        # ACEStepPipeline takes an integer GPU id; it falls back to CPU if CUDA
        # is unavailable. Parse a device string like "cuda:0" or "cuda".
        if DEVICE.startswith("cuda"):
            device_id = int(DEVICE.split(":", 1)[1]) if ":" in DEVICE else 0
            dtype = "bfloat16"
        else:
            device_id = 0
            dtype = "float32"

        pipeline = ACEStepPipeline(
            checkpoint_dir=checkpoint,
            device_id=device_id,
            dtype=dtype,
            torch_compile=False,
            cpu_offload=False,
            overlapped_decode=False,
        )
        logger.info("ACE-Step pipeline created.")
        return pipeline
    except Exception as exc:  # noqa: BLE001
        msg = _format_model_error("ace_step", exc)
        logger.error(msg)
        logger.debug(traceback.format_exc())
        raise RuntimeError(msg) from exc


def _generate_music_ace_step(
    prompt: str,
    duration_seconds: float,
    lyrics: str | None = None,
) -> tuple[np.ndarray, int]:
    """Generate music with ACE-Step and return (audio_array, sample_rate)."""
    pipeline = _model_cache.setdefault("ace_step", _load_ace_step())

    # ACE-Step outputs stereo 48 kHz audio. The public __call__ API always
    # writes to disk, so we use a temporary WAV file and load it back.
    sample_rate = 48000
    temp_path = OUTPUT_DIR / "music" / f"_ace_step_tmp_{_now_str()}.wav"
    temp_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        result = pipeline(
            format="wav",
            audio_duration=duration_seconds,
            prompt=prompt,
            lyrics=lyrics if lyrics is not None else "",
            save_path=str(temp_path),
            task="text2music",
            batch_size=1,
        )

        if isinstance(result, (list, tuple)) and len(result) > 0:
            output_path = Path(result[0])
        else:
            output_path = temp_path

        audio, sr = sf.read(str(output_path), dtype=np.float32)
        return np.asarray(audio), int(sr)
    finally:
        try:
            if temp_path.exists():
                temp_path.unlink()
        except OSError:
            pass


# ---------------------------------------------------------------------------
# Stable Audio Open wrapper (SFX / ambient)
# ---------------------------------------------------------------------------


def _load_stable_audio_open() -> Any:
    """Lazy load Stable Audio Open pipeline."""
    try:
        from stable_audio_tools import get_pretrained_model  # type: ignore

        logger.info("Loading Stable Audio Open on %s ...", DEVICE)
        pretrained_name = CONFIG["models"]["stable_audio_open"].get("pretrained_name")
        if not pretrained_name:
            raise ValueError("No pretrained_name configured for stable_audio_open")

        if Path(pretrained_name).exists() and not (Path(pretrained_name) / "model_config.json").exists():
            raise FileNotFoundError(
                f"Stable Audio Open local path is missing model_config.json: {pretrained_name}"
            )

        model, model_config = get_pretrained_model(pretrained_name)
        model.to(DEVICE).eval().requires_grad_(False)

        sample_rate = model_config.get("sample_rate", 44100)
        sample_size = model_config.get("sample_size", sample_rate * 47)
        logger.info("Stable Audio Open loaded (sr=%d, sample_size=%d).", sample_rate, sample_size)
        return {
            "model": model,
            "model_config": model_config,
            "sample_rate": sample_rate,
            "sample_size": sample_size,
        }
    except Exception as exc:  # noqa: BLE001
        msg = _format_model_error("stable_audio_open", exc)
        logger.error(msg)
        logger.debug(traceback.format_exc())
        raise RuntimeError(msg) from exc


def _generate_sfx_stable_audio_open(prompt: str, duration_seconds: float) -> tuple[np.ndarray, int]:
    state = _model_cache.setdefault("stable_audio_open", _load_stable_audio_open())
    model = state["model"]
    sample_rate = state["sample_rate"]
    sample_size = state["sample_size"]

    from stable_audio_tools.inference.generation import generate_diffusion_cond  # type: ignore

    max_seconds = sample_size / sample_rate
    if duration_seconds > max_seconds:
        logger.warning(
            "Requested SFX duration %.1fs exceeds model max %.1fs; clamping.",
            duration_seconds,
            max_seconds,
        )
        duration_seconds = max_seconds

    conditioning = [{
        "prompt": prompt,
        "seconds_start": 0,
        "seconds_total": duration_seconds,
    }]

    output = generate_diffusion_cond(
        model,
        steps=100,
        cfg_scale=7,
        conditioning=conditioning,
        sample_size=sample_size,
        sigma_min=0.3,
        sigma_max=500,
        sampler_type="dpmpp-3m-sde",
        device=DEVICE,
        batch_size=1,
    )

    audio = output.detach().cpu().float().numpy()
    if audio.ndim == 3:
        audio = audio[0]
    if audio.ndim == 2:
        audio = audio.T if audio.shape[0] <= 2 else audio

    target_samples = int(duration_seconds * sample_rate)
    audio = audio[:target_samples]
    audio = np.clip(np.asarray(audio, dtype=np.float32), -1.0, 1.0)
    return audio, int(sample_rate)


# ---------------------------------------------------------------------------
# MMAudio wrapper (Foley / video-to-audio)
# ---------------------------------------------------------------------------


def _load_mmaudio() -> Any:
    """Lazy load MMAudio."""
    try:
        torch.backends.cuda.matmul.allow_tf32 = True
        torch.backends.cudnn.allow_tf32 = True

        from mmaudio.eval_utils import all_model_cfg  # type: ignore
        from mmaudio.model.networks import get_my_mmaudio  # type: ignore
        from mmaudio.model.utils.features_utils import FeaturesUtils  # type: ignore

        logger.info("Loading MMAudio on %s ...", DEVICE)
        model_name = CONFIG["models"]["mmaudio"].get("model_name", "large_44k_v2")

        if model_name not in all_model_cfg:
            raise ValueError(f"Unknown MMAudio variant: {model_name}")

        model_cfg = all_model_cfg[model_name]
        model_cfg.download_if_needed()
        seq_cfg = model_cfg.seq_cfg

        dtype = torch.float32 if DEVICE == "cpu" else torch.bfloat16

        for required_key, required_path in (
            ("model_path", model_cfg.model_path),
            ("vae_path", model_cfg.vae_path),
            ("synchformer_ckpt", model_cfg.synchformer_ckpt),
        ):
            if required_path and not Path(required_path).exists():
                raise FileNotFoundError(
                    f"MMAudio required file missing ({required_key}): {required_path}"
                )

        net = get_my_mmaudio(model_cfg.model_name).to(DEVICE, dtype).eval()
        net.load_weights(
            torch.load(
                model_cfg.model_path,
                map_location=DEVICE,
                weights_only=True,
            )
        )
        logger.info("Loaded MMAudio weights from %s", model_cfg.model_path)

        feature_utils = FeaturesUtils(
            tod_vae_ckpt=model_cfg.vae_path,
            synchformer_ckpt=model_cfg.synchformer_ckpt,
            enable_conditions=True,
            mode=model_cfg.mode,
            bigvgan_vocoder_ckpt=model_cfg.bigvgan_16k_path,
            need_vae_encoder=False,
        ).to(DEVICE, dtype).eval()

        logger.info("MMAudio loaded (%s, sr=%d).", model_name, seq_cfg.sampling_rate)
        return {
            "net": net,
            "feature_utils": feature_utils,
            "seq_cfg": seq_cfg,
            "model_name": model_name,
        }
    except Exception as exc:  # noqa: BLE001
        msg = _format_model_error("mmaudio", exc)
        logger.error(msg)
        logger.debug(traceback.format_exc())
        raise RuntimeError(msg) from exc


@torch.inference_mode()
def _generate_foley_mmaudio(
    prompt: str,
    duration_seconds: float,
    video_path: str | None = None,
) -> tuple[np.ndarray, int]:
    state = _model_cache.setdefault("mmaudio", _load_mmaudio())
    net = state["net"]
    feature_utils = state["feature_utils"]
    seq_cfg = state["seq_cfg"]

    from mmaudio.eval_utils import generate, load_video  # type: ignore
    from mmaudio.model.flow_matching import FlowMatching  # type: ignore

    if video_path:
        video_info = load_video(Path(video_path), duration_seconds)
        clip_frames = video_info.clip_frames.unsqueeze(0)
        sync_frames = video_info.sync_frames.unsqueeze(0)
        duration = video_info.duration_sec
    else:
        clip_frames = sync_frames = None
        duration = duration_seconds

    seq_cfg.duration = duration
    net.update_seq_lengths(seq_cfg.latent_seq_len, seq_cfg.clip_seq_len, seq_cfg.sync_seq_len)

    rng = torch.Generator(device=DEVICE).manual_seed(42)
    fm = FlowMatching(min_sigma=0, inference_mode="euler", num_steps=25)

    audios = generate(
        clip_frames,
        sync_frames,
        [prompt],
        negative_text=[""],
        feature_utils=feature_utils,
        net=net,
        fm=fm,
        rng=rng,
        cfg_strength=4.5,
    )

    audio = audios.float().cpu()[0]
    audio_np = audio.numpy()
    if audio_np.ndim > 1:
        audio_np = audio_np.squeeze()
    return audio_np, int(seq_cfg.sampling_rate)


# ---------------------------------------------------------------------------
# Request / response schemas
# ---------------------------------------------------------------------------

class MusicRequest(BaseModel):
    prompt: str = Field(..., description="Text prompt / tags for the music.")
    duration_seconds: float = Field(..., gt=0, description="Target duration in seconds.")
    lyrics: str | None = Field(None, description="Optional lyrics for vocal tracks.")
    output_format: str = Field("wav", description="Output format; currently only 'wav' is supported.")


class SFXRequest(BaseModel):
    prompt: str = Field(..., description="Text prompt for the sound effect.")
    duration_seconds: float = Field(..., gt=0, description="Target duration in seconds.")


class FoleyRequest(BaseModel):
    prompt: str = Field(..., description="Text prompt describing the desired Foley audio.")
    duration_seconds: float = Field(..., gt=0, description="Target duration in seconds.")
    video_path: str | None = Field(None, description="Optional local video path; if omitted, text-to-audio is used.")


class GenerationResponse(BaseModel):
    success: bool
    model: str
    file_path: str
    audio_base64: str
    sample_rate: int
    duration_seconds: float
    prompt: str
    message: str


class HealthResponse(BaseModel):
    status: str
    device: str
    models: dict[str, dict[str, Any]]


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------

@app.get("/")
def root() -> dict[str, str]:
    return {"message": "UnrealMCP Audio Generation Service"}


@app.post("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    """Return service health and per-model availability.

    Models are loaded lazily, so this endpoint only reports whether each model
    *can* be imported/loaded based on earlier attempts (or not yet attempted).
    """
    models_status = {}
    for name in ("ace_step", "stable_audio_open", "mmaudio"):
        if name in _model_cache:
            models_status[name] = {"available": True, "error": None}
        elif name in _model_errors:
            models_status[name] = {"available": False, "error": _model_errors[name]}
        else:
            models_status[name] = {"available": False, "error": "not loaded yet"}

    return HealthResponse(
        status="ok",
        device=DEVICE,
        models=models_status,
    )


@app.on_event("startup")
def startup_health_check() -> None:
    """Log configuration and verify the output directory on startup."""
    logger.info("Audio server starting on %s:%d (device=%s)", CONFIG["host"], CONFIG["port"], DEVICE)
    logger.info("Output directory: %s", OUTPUT_DIR)
    _ensure_output_dir("health")


def _handle_generation(
    model_name: str,
    prompt: str,
    duration_seconds: float,
    output_path: Path,
    generate_fn: callable,
) -> GenerationResponse:
    """Common generation pipeline: run model, save WAV, return base64."""
    start = time.time()
    try:
        audio, sample_rate = generate_fn()
        _save_wav(audio, sample_rate, output_path)
        elapsed = time.time() - start
        logger.info("Generated %s in %.2fs -> %s", model_name, elapsed, output_path)
        return GenerationResponse(
            success=True,
            model=model_name,
            file_path=str(output_path),
            audio_base64=_audio_to_base64(output_path),
            sample_rate=sample_rate,
            duration_seconds=duration_seconds,
            prompt=prompt,
            message=f"Generated {model_name} audio in {elapsed:.2f}s.",
        )
    except HTTPException:
        raise
    except RuntimeError as exc:
        # Likely a model-load failure surfaced by the lazy loader; message was
        # already formatted by _format_model_error.
        logger.error("%s generation failed: %s", model_name, exc)
        logger.debug(traceback.format_exc())
        _model_errors[model_name] = str(exc)
        raise HTTPException(
            status_code=HTTPStatus.SERVICE_UNAVAILABLE,
            detail=str(exc),
        ) from exc
    except Exception as exc:  # noqa: BLE001
        logger.error("%s generation failed: %s", model_name, exc)
        logger.debug(traceback.format_exc())
        _model_errors[model_name] = str(exc)
        detail = _format_model_error(model_name, exc)
        raise HTTPException(
            status_code=HTTPStatus.INTERNAL_SERVER_ERROR,
            detail=detail,
        ) from exc


@app.post("/generate/music", response_model=GenerationResponse)
def generate_music(request: MusicRequest) -> GenerationResponse:
    output_path = _ensure_output_dir("music") / f"music_{_now_str()}.wav"
    return _handle_generation(
        model_name="ace_step",
        prompt=request.prompt,
        duration_seconds=request.duration_seconds,
        output_path=output_path,
        generate_fn=lambda: _generate_music_ace_step(
            prompt=request.prompt,
            duration_seconds=request.duration_seconds,
            lyrics=request.lyrics,
        ),
    )


@app.post("/generate/sfx", response_model=GenerationResponse)
def generate_sfx(request: SFXRequest) -> GenerationResponse:
    output_path = _ensure_output_dir("sfx") / f"sfx_{_now_str()}.wav"
    return _handle_generation(
        model_name="stable_audio_open",
        prompt=request.prompt,
        duration_seconds=request.duration_seconds,
        output_path=output_path,
        generate_fn=lambda: _generate_sfx_stable_audio_open(
            prompt=request.prompt,
            duration_seconds=request.duration_seconds,
        ),
    )


@app.post("/generate/foley", response_model=GenerationResponse)
def generate_foley(request: FoleyRequest) -> GenerationResponse:
    if request.video_path and not Path(request.video_path).exists():
        raise HTTPException(status_code=400, detail=f"video_path not found: {request.video_path}")

    output_path = _ensure_output_dir("foley") / f"foley_{_now_str()}.wav"
    return _handle_generation(
        model_name="mmaudio",
        prompt=request.prompt,
        duration_seconds=request.duration_seconds,
        output_path=output_path,
        generate_fn=lambda: _generate_foley_mmaudio(
            prompt=request.prompt,
            duration_seconds=request.duration_seconds,
            video_path=request.video_path,
        ),
    )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host=CONFIG["host"],
        port=CONFIG["port"],
        log_level="info",
        reload=False,
    )
