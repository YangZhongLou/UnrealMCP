#!/usr/bin/env python3
"""Minimal reproduction script for ACE-Step meta-tensor load failure.

Run directly in the audio-server venv:

    D:/Playground/TA-Playground/Plugins/UnrealMCP/.venv/Scripts/python.exe \
        audio_server/tests/debug_ace_step.py
"""

from __future__ import annotations

import os
import sys
import time
import traceback
from pathlib import Path

# Make the editable ACE-Step package importable.
ACE_STEP_DIR = Path(__file__).resolve().parent.parent.parent / "third_party" / "ACE-Step"
if str(ACE_STEP_DIR) not in sys.path:
    sys.path.insert(0, str(ACE_STEP_DIR))

from acestep.pipeline_ace_step import ACEStepPipeline  # noqa: E402


def run(
    duration: float = 5.0,
    device_id: int = 0,
    dtype: str = "bfloat16",
    cpu_offload: bool = False,
    checkpoint_dir: str | None = None,
) -> None:
    if checkpoint_dir is None:
        checkpoint_dir = os.path.expanduser("~/.cache/ace-step/checkpoints")

    print(f"device_id={device_id}, dtype={dtype}, cpu_offload={cpu_offload}")
    print(f"checkpoint_dir={checkpoint_dir}")
    print(f"duration={duration}s")

    output_dir = Path(__file__).resolve().parent.parent / "output" / "debug"
    output_dir.mkdir(parents=True, exist_ok=True)
    save_path = output_dir / f"debug_{duration}s_{dtype}_cpuoffload{cpu_offload}.wav"

    start = time.time()
    try:
        pipeline = ACEStepPipeline(
            checkpoint_dir=checkpoint_dir,
            device_id=device_id,
            dtype=dtype,
            torch_compile=False,
            cpu_offload=cpu_offload,
            overlapped_decode=False,
        )
        print(f"Pipeline created in {time.time() - start:.2f}s")

        gen_start = time.time()
        result = pipeline(
            format="wav",
            audio_duration=duration,
            prompt="upbeat electronic loop, 120 BPM",
            lyrics="",
            save_path=str(save_path),
            task="text2music",
            batch_size=1,
        )
        elapsed = time.time() - gen_start
        print(f"Generation succeeded in {elapsed:.2f}s")
        print(f"Result: {result}")
        print(f"Output exists: {Path(save_path).exists()}, size: {Path(save_path).stat().st_size if Path(save_path).exists() else 0}")
    except Exception as exc:  # noqa: BLE001
        print(f"FAILED: {type(exc).__name__}: {exc}")
        traceback.print_exc()
        raise


if __name__ == "__main__":
    # Default mirrors audio_server/main.py settings.
    run(
        duration=float(os.getenv("DEBUG_DURATION", "5")),
        device_id=int(os.getenv("DEBUG_DEVICE_ID", "0")),
        dtype=os.getenv("DEBUG_DTYPE", "bfloat16"),
        cpu_offload=os.getenv("DEBUG_CPU_OFFLOAD", "0") == "1",
    )
