#!/usr/bin/env python3
"""Self-iterating AI audio asset orchestrator for TA-Playground.

Generates music/SFX/foley via the local FastAPI audio server, evaluates each
iteration with simple audio metrics, refines the prompt automatically, and
delivers the best WAV file plus a JSON report.
"""

import argparse
import base64
import io
import json
import math
import os
import shutil
import sys
import time
from typing import Any, Dict, List, Tuple

import numpy as np
import requests
import soundfile as sf

ENDPOINTS = {
    "music": "/generate/music",
    "sfx": "/generate/sfx",
    "foley": "/generate/foley",
}

CLIP_THRESHOLD = 0.001  # fraction of samples considered significant clipping


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Self-iterating AI audio asset generator"
    )
    parser.add_argument(
        "--type",
        choices=["music", "sfx", "foley"],
        required=True,
        help="Asset type to generate",
    )
    parser.add_argument(
        "--prompt",
        required=True,
        help="English generation prompt",
    )
    parser.add_argument(
        "--duration",
        type=float,
        required=True,
        help="Target duration in seconds",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        help="Directory to save iteration WAV files",
    )
    parser.add_argument(
        "--server-url",
        default="http://127.0.0.1:8123",
        help="Base URL of the audio generation server",
    )
    parser.add_argument(
        "--max-iterations",
        type=int,
        default=3,
        help="Maximum generation/evaluation iterations",
    )
    parser.add_argument(
        "--target-loudness-db",
        type=float,
        default=-18.0,
        help="Target RMS loudness in dB",
    )
    parser.add_argument(
        "--loudness-tol-db",
        type=float,
        default=4.0,
        help="Acceptable loudness deviation in dB",
    )
    parser.add_argument(
        "--duration-tol",
        type=float,
        default=0.2,
        help="Acceptable duration deviation in seconds",
    )
    parser.add_argument(
        "--video-path",
        default=None,
        help="Optional video path for foley generation",
    )
    return parser.parse_args()


def generate_audio(
    server_url: str,
    asset_type: str,
    prompt: str,
    duration: float,
    video_path: str | None,
) -> Tuple[Any, Dict[str, Any]]:
    """Call the audio server and return an audio source (path or BytesIO) and metadata."""
    url = f"{server_url.rstrip('/')}{ENDPOINTS[asset_type]}"
    payload: Dict[str, Any] = {
        "prompt": prompt,
        "duration_seconds": duration,
        "output_format": "wav",
    }
    if asset_type == "foley" and video_path:
        payload["video_path"] = video_path

    response = requests.post(url, json=payload, timeout=600)
    response.raise_for_status()
    data = response.json()

    if data.get("success") is False:
        raise RuntimeError(data.get("message", "audio generation failed"))

    audio_base64 = data.get("audio_base64")
    if audio_base64:
        wav_bytes = base64.b64decode(audio_base64)
        return io.BytesIO(wav_bytes), data

    file_path = data.get("file_path")
    if file_path and os.path.exists(file_path):
        return file_path, data

    raise RuntimeError(
        "server response did not contain usable audio (no file_path or audio_base64)"
    )


def read_audio(source: Any) -> Tuple[np.ndarray, int]:
    """Read audio from a file path or file-like object."""
    data, samplerate = sf.read(source, dtype="float64")
    return data, samplerate


def save_iteration_source(source: Any, destination: str) -> None:
    """Preserve the generated audio to the iteration output path."""
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    if isinstance(source, str) and os.path.exists(source):
        shutil.copy2(source, destination)
    else:
        data, samplerate = read_audio(source)
        sf.write(destination, data, samplerate, subtype="PCM_16")


def compute_metrics(data: np.ndarray, samplerate: int) -> Dict[str, Any]:
    """Compute basic audio quality metrics."""
    if data.ndim == 1:
        data = data[:, np.newaxis]

    samples, channels = data.shape
    flat = data.flatten()

    duration_seconds = float(samples / samplerate)
    peak = float(np.max(np.abs(flat)))
    rms = float(np.sqrt(np.mean(flat**2)))
    rms_db = 20.0 * math.log10(rms) if rms > 0.0 else -float("inf")
    clipping_ratio = float(np.mean(np.abs(flat) >= 1.0))

    # Spectral centroid averaged across channels.
    centroids = []
    for ch in range(channels):
        x = data[:, ch]
        spectrum = np.fft.rfft(x)
        magnitude = np.abs(spectrum)
        frequencies = np.fft.rfftfreq(len(x), 1.0 / samplerate)
        if magnitude.sum() > 0.0:
            centroid = float(np.sum(frequencies * magnitude) / np.sum(magnitude))
        else:
            centroid = 0.0
        centroids.append(centroid)
    spectral_centroid_hz = float(np.mean(centroids))

    # Zero-crossing rate averaged across channels.
    zero_crossings = np.sum((data[1:, :] * data[:-1, :]) < 0, axis=1) / channels
    zero_crossing_rate = float(np.mean(zero_crossings))

    return {
        "duration_seconds": duration_seconds,
        "peak": peak,
        "rms": rms,
        "rms_db": rms_db,
        "clipping_ratio": clipping_ratio,
        "spectral_centroid_hz": spectral_centroid_hz,
        "zero_crossing_rate": zero_crossing_rate,
    }


def evaluate_metrics(
    metrics: Dict[str, Any],
    target_duration: float,
    target_loudness_db: float,
    loudness_tol_db: float,
    duration_tol: float,
) -> Tuple[bool, List[str]]:
    """Return (accepted, list of issue labels)."""
    issues: List[str] = []

    duration_ok = abs(metrics["duration_seconds"] - target_duration) <= duration_tol
    if not duration_ok:
        issues.append("duration")

    loudness_ok = abs(metrics["rms_db"] - target_loudness_db) <= loudness_tol_db
    if not loudness_ok:
        if metrics["rms_db"] < target_loudness_db - loudness_tol_db:
            issues.append("too-quiet")
        else:
            issues.append("too-loud")

    clipping_ok = metrics["clipping_ratio"] <= CLIP_THRESHOLD
    if not clipping_ok:
        issues.append("clipping")

    accepted = duration_ok and loudness_ok and clipping_ok
    return accepted, issues


def refine_prompt(
    prompt: str,
    issues: List[str],
    metrics: Dict[str, Any],
    target_duration: float,
    target_loudness_db: float,
    loudness_tol_db: float,
    duration_tol: float,
) -> str:
    """Apply simple prompt refinement rules based on detected issues."""
    modifiers: List[str] = []

    if "too-quiet" in issues:
        modifiers.extend(["loud", "close-miked"])
    if "too-loud" in issues:
        modifiers.extend(["normalized", "gentle level"])
    if "clipping" in issues:
        modifiers.extend(["clean", "headroom"])

    if "duration" in issues:
        if metrics["duration_seconds"] < target_duration - duration_tol:
            modifiers.extend(["extended", "longer tail"])
        else:
            modifiers.extend(["short", "tight cutoff"])

    # Fallback if somehow no modifier was picked.
    if not modifiers:
        modifiers.append("high quality")

    deduped = []
    for mod in modifiers:
        if mod.lower() not in prompt.lower():
            deduped.append(mod)

    if not deduped:
        return prompt

    return f"{prompt}, {', '.join(deduped)}"


def score_metrics(
    metrics: Dict[str, Any],
    target_duration: float,
    target_loudness_db: float,
) -> float:
    """Lower is better. Used to pick the best iteration when none are accepted."""
    duration_error = abs(metrics["duration_seconds"] - target_duration)
    loudness_error = (
        abs(metrics["rms_db"] - target_loudness_db)
        if math.isfinite(metrics["rms_db"])
        else 999.0
    )
    clipping_penalty = metrics["clipping_ratio"] * 1000.0
    return duration_error + (loudness_error * 0.1) + clipping_penalty


def main() -> int:
    args = parse_args()
    os.makedirs(args.output_dir, exist_ok=True)

    run_timestamp = time.strftime("%Y%m%d_%H%M%S")
    prompt = args.prompt
    history: List[Dict[str, Any]] = []
    best_entry: Dict[str, Any] | None = None

    for iteration in range(1, args.max_iterations + 1):
        iter_timestamp = time.strftime("%Y%m%d_%H%M%S")
        print(
            f"[iter {iteration}/{args.max_iterations}] generating: {prompt}",
            file=sys.stderr,
        )

        try:
            source, metadata = generate_audio(
                args.server_url,
                args.type,
                prompt,
                args.duration,
                args.video_path,
            )
        except Exception as exc:
            print(f"[iter {iteration}] generation failed: {exc}", file=sys.stderr)
            history.append(
                {
                    "iteration": iteration,
                    "prompt": prompt,
                    "file_path": None,
                    "metrics": None,
                    "accepted": False,
                    "issues": ["generation-error"],
                    "error": str(exc),
                }
            )
            continue

        out_name = f"asset_iter{iteration}_{iter_timestamp}.wav"
        out_path = os.path.join(args.output_dir, out_name)
        save_iteration_source(source, out_path)

        data, samplerate = read_audio(out_path)
        metrics = compute_metrics(data, samplerate)

        accepted, issues = evaluate_metrics(
            metrics,
            args.duration,
            args.target_loudness_db,
            args.loudness_tol_db,
            args.duration_tol,
        )

        entry = {
            "iteration": iteration,
            "prompt": prompt,
            "file_path": out_path,
            "metrics": metrics,
            "accepted": accepted,
            "issues": issues,
        }
        history.append(entry)

        if best_entry is None or score_metrics(
            metrics, args.duration, args.target_loudness_db
        ) < score_metrics(best_entry["metrics"], args.duration, args.target_loudness_db):
            best_entry = entry

        print(
            f"[iter {iteration}] metrics={json.dumps(metrics, ensure_ascii=False)} accepted={accepted} issues={issues}",
            file=sys.stderr,
        )

        if accepted:
            break

        prompt = refine_prompt(
            prompt,
            issues,
            metrics,
            args.duration,
            args.target_loudness_db,
            args.loudness_tol_db,
            args.duration_tol,
        )

    if best_entry is None:
        print(json.dumps({"success": False, "error": "all iterations failed"}, indent=2))
        return 1

    final_name = f"asset_final_{run_timestamp}.wav"
    final_path = os.path.join(args.output_dir, final_name)
    shutil.copy2(best_entry["file_path"], final_path)

    prompt_history = [entry["prompt"] for entry in history if entry.get("metrics")]
    accepted = best_entry["accepted"]
    message = (
        f"Accepted at iteration {best_entry['iteration']}."
        if accepted
        else f"Max iterations reached; best iteration {best_entry['iteration']} selected with issues {best_entry['issues']}."
    )

    report = {
        "success": accepted,
        "best_iteration": best_entry["iteration"],
        "final_file": final_path,
        "metrics": best_entry["metrics"],
        "prompt_history": prompt_history,
        "accepted": accepted,
        "message": message,
        "iterations": history,
    }

    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if accepted else 0  # deliver best even if not accepted; caller checks `accepted`


if __name__ == "__main__":
    sys.exit(main())
