from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def _pythonpath_env(repo_root: Path) -> dict[str, str]:
    env = os.environ.copy()
    src = str(repo_root / "deepfake_detector" / "src")
    env["PYTHONPATH"] = src + os.pathsep + env.get("PYTHONPATH", "")
    return env


def _write_image(path: Path) -> None:
    arr = np.random.default_rng(7).integers(0, 255, size=(48, 48, 3), dtype=np.uint8)
    Image.fromarray(arr).save(path)


def test_cli_scan_json_output(tmp_path: Path) -> None:
    repo_root = Path(__file__).resolve().parents[2]
    image_path = tmp_path / "sample.png"
    _write_image(image_path)

    completed = subprocess.run(
        [
            sys.executable,
            "-m",
            "deepfake_detector.cli",
            "scan",
            str(image_path),
            "--json",
        ],
        check=False,
        capture_output=True,
        text=True,
        env=_pythonpath_env(repo_root),
    )

    assert completed.returncode == 0, completed.stderr
    payload = json.loads(completed.stdout)
    assert "authenticity_score" in payload
    assert "confidence" in payload
    assert "signals" in payload


def test_cli_batch_fails_when_no_supported_files(tmp_path: Path) -> None:
    repo_root = Path(__file__).resolve().parents[2]
    (tmp_path / "note.txt").write_text("nothing", encoding="utf-8")

    completed = subprocess.run(
        [
            sys.executable,
            "-m",
            "deepfake_detector.cli",
            "batch",
            str(tmp_path),
        ],
        check=False,
        capture_output=True,
        text=True,
        env=_pythonpath_env(repo_root),
    )

    assert completed.returncode == 2
    assert "No supported media files found" in completed.stderr
