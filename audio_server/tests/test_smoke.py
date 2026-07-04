#!/usr/bin/env python3
"""Smoke tests for the UnrealMCP audio generation server.

These tests exercise the server's health endpoint and, optionally, its three
generation endpoints. They are intended to run quickly when ``--skip-generation``
is passed; generation calls run real model inference and may take minutes.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime
from pathlib import Path
from typing import Any

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8123


def _now_str() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def _load_dotenv(audio_server_dir: Path) -> None:
    """Load ``audio_server/.env`` if it exists.

    Uses ``python-dotenv`` when available; otherwise falls back to a minimal
    line parser so the smoke test has no hard dependencies beyond the stdlib.
    """
    env_path = audio_server_dir / ".env"
    if not env_path.exists():
        return

    try:
        from dotenv import load_dotenv

        load_dotenv(dotenv_path=env_path, override=False, verbose=False)
    except ImportError:
        # Minimal fallback parser for KEY=VALUE lines.
        with env_path.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, _, value = line.partition("=")
                key = key.strip()
                value = value.strip().strip('"').strip("'")
                if key and key not in os.environ:
                    os.environ[key] = value


def _get_base_url(audio_server_dir: Path) -> str:
    """Return the server base URL from ``.env`` / environment or defaults."""
    _load_dotenv(audio_server_dir)
    host = os.getenv("AUDIO_SERVER_HOST", DEFAULT_HOST)
    port = int(os.getenv("AUDIO_SERVER_PORT", str(DEFAULT_PORT)))
    return f"http://{host}:{port}"


def _request(
    method: str,
    url: str,
    data: dict[str, Any] | None = None,
    timeout: int = 30,
) -> tuple[int, Any]:
    """Make a JSON request and return (status_code, parsed_body)."""
    headers = {"Accept": "application/json"}
    body = None
    if data is not None:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"

    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            status = resp.status
            raw = resp.read()
    except urllib.error.HTTPError as exc:
        status = exc.code
        raw = exc.read()

    if not raw:
        return status, None
    try:
        return status, json.loads(raw.decode("utf-8"))
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Non-JSON response (HTTP {status}): {exc}") from exc


def _run_health(base_url: str) -> tuple[bool, list[str]]:
    """Call ``/health`` and print a short status report."""
    status, body = _request("POST", f"{base_url}/health")
    if status != 200 or not isinstance(body, dict):
        return False, [f"  /health failed: HTTP {status}, body={body}"]

    device = body.get("device", "unknown")
    models = body.get("models", {})
    lines = [
        f"  device: {device}",
        "  model availability:",
    ]
    for name, info in models.items():
        available = info.get("available", False) if isinstance(info, dict) else False
        error = info.get("error") if isinstance(info, dict) else None
        lines.append(f"    - {name}: {'available' if available else 'not available'} {error or ''}".rstrip())

    return True, lines


def _run_generation(
    base_url: str,
    name: str,
    payload: dict[str, Any],
    output_dir: Path,
    timeout: int = 600,
) -> tuple[bool, list[str]]:
    """Call a generation endpoint, save the WAV, and verify it is non-empty."""
    url = f"{base_url}/generate/{name}"
    start = time.time()
    status, body = _request("POST", url, data=payload, timeout=timeout)
    elapsed = time.time() - start

    if status != 200:
        return False, [f"  /generate/{name} failed: HTTP {status}, body={body}"]
    if not isinstance(body, dict):
        return False, [f"  /generate/{name} returned non-JSON body"]
    if not body.get("success"):
        return False, [f"  /generate/{name} returned success=false: {body.get('message')}"]

    b64_audio = body.get("audio_base64")
    if not isinstance(b64_audio, str):
        return False, [f"  /generate/{name} missing audio_base64"]

    output_dir.mkdir(parents=True, exist_ok=True)
    wav_path = output_dir / f"{name}_test_{_now_str()}.wav"
    try:
        wav_bytes = base64.b64decode(b64_audio)
        wav_path.write_bytes(wav_bytes)
    except Exception as exc:  # noqa: BLE001
        return False, [f"  /generate/{name} failed to save WAV: {exc}"]

    if wav_path.stat().st_size == 0:
        return False, [f"  /generate/{name} saved an empty WAV file: {wav_path}"]

    return True, [
        f"  /generate/{name}: success ({elapsed:.1f}s) -> {wav_path} ({wav_path.stat().st_size} bytes)"
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Smoke-test the UnrealMCP audio generation server."
    )
    parser.add_argument(
        "--skip-generation",
        action="store_true",
        help="Skip model-inference generation calls (runs health checks only).",
    )
    args = parser.parse_args()

    audio_server_dir = Path(__file__).resolve().parent.parent
    base_url = _get_base_url(audio_server_dir)
    output_dir = audio_server_dir / "output" / "tests"

    print(f"Testing audio server at {base_url}")
    failures: list[str] = []
    successes: list[str] = []

    # GET /
    status, body = _request("GET", base_url)
    if status == 200 and isinstance(body, dict) and "message" in body:
        successes.append(f"  GET /: {body['message']}")
    else:
        failures.append(f"  GET / failed: HTTP {status}, body={body}")

    # POST /health
    ok, health_lines = _run_health(base_url)
    if ok:
        successes.extend(health_lines)
    else:
        failures.extend(health_lines)

    if not args.skip_generation:
        generation_jobs = [
            (
                "music",
                {
                    "prompt": "upbeat electronic loop",
                    "duration_seconds": 5,
                    "lyrics": None,
                },
            ),
            (
                "sfx",
                {
                    "prompt": "short beep",
                    "duration_seconds": 3,
                },
            ),
            (
                "foley",
                {
                    "prompt": "footsteps on wood",
                    "duration_seconds": 5,
                    "video_path": None,
                },
            ),
        ]

        for name, payload in generation_jobs:
            try:
                ok, lines = _run_generation(base_url, name, payload, output_dir)
            except Exception as exc:  # noqa: BLE001
                ok = False
                lines = [f"  /generate/{name} raised an exception: {exc}"]
            if ok:
                successes.extend(lines)
            else:
                failures.extend(lines)
    else:
        successes.append("  Generation calls skipped (--skip-generation).")

    # Summary
    print("\nResults:")
    for line in successes:
        print(f"  [OK] {line}")
    for line in failures:
        print(f"  [FAIL] {line}")

    print(
        f"\nSummary: {len(successes)} passed, {len(failures)} failed"
        f" ({'generation skipped' if args.skip_generation else 'generation enabled'})"
    )

    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
