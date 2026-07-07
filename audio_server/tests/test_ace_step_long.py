#!/usr/bin/env python3
"""Long-duration ACE-Step music generation test via the running audio server."""

from __future__ import annotations

import base64
import os
import sys
import time
from pathlib import Path

import requests

BASE_URL = "http://127.0.0.1:8123"
OUTPUT_DIR = Path("D:/Playground/TA-Playground/Plugins/UnrealMCP/audio_server/output/music")

PROMPT = "cinematic orchestral, tense, strings and brass, 110 BPM, instrumental"


def _save_base64_wav(b64_audio: str, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    wav_bytes = base64.b64decode(b64_audio)
    path.write_bytes(wav_bytes)


def post_health() -> None:
    print("\n[health] POST /health")
    resp = requests.post(f"{BASE_URL}/health", timeout=30)
    resp.raise_for_status()
    print(resp.json())


def generate_and_save(duration_seconds: int, timeout: int, output_path: Path) -> None:
    print(f"\n[generate] POST /generate/music duration={duration_seconds}s timeout={timeout}s")
    payload = {
        "prompt": PROMPT,
        "duration_seconds": duration_seconds,
    }
    start = time.time()
    resp = requests.post(
        f"{BASE_URL}/generate/music",
        json=payload,
        timeout=timeout,
    )
    elapsed = time.time() - start
    resp.raise_for_status()
    body = resp.json()

    if not body.get("success"):
        raise RuntimeError(f"Server reported failure: {body.get('message')}")

    b64_audio = body.get("audio_base64")
    if not isinstance(b64_audio, str):
        raise RuntimeError("Response missing audio_base64")

    _save_base64_wav(b64_audio, output_path)
    file_size = os.path.getsize(output_path)
    sample_rate = body.get("sample_rate", "unknown")

    print(f"  success: True")
    print(f"  elapsed: {elapsed:.2f}s")
    print(f"  file: {output_path}")
    print(f"  file size: {file_size} bytes")
    print(f"  sample rate: {sample_rate} Hz")


def main() -> int:
    try:
        post_health()

        generate_and_save(
            duration_seconds=30,
            timeout=600,
            output_path=OUTPUT_DIR / "test_30s.wav",
        )

        generate_and_save(
            duration_seconds=180,
            timeout=3600,
            output_path=OUTPUT_DIR / "test_180s.wav",
        )

        print("\n[done] Both generations completed successfully.")
        return 0
    except Exception as exc:  # noqa: BLE001
        print(f"\n[error] {type(exc).__name__}: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
