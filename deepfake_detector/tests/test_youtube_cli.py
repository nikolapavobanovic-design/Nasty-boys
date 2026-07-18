"""Tests for the youtube subcommand and youtube.py helpers."""
from __future__ import annotations

import contextlib
import json
import os
import sys
from io import StringIO
from pathlib import Path
from unittest.mock import patch

import numpy as np
from PIL import Image


def _pythonpath_env(repo_root: Path) -> dict[str, str]:
    env = os.environ.copy()
    src = str(repo_root / "deepfake_detector" / "src")
    env["PYTHONPATH"] = src + os.pathsep + env.get("PYTHONPATH", "")
    return env


def _write_gif(path: Path) -> None:
    frames = []
    for i in range(3):
        frame = np.zeros((32, 32, 3), dtype=np.uint8)
        frame[:, :, 0] = (i * 80) % 255
        frames.append(Image.fromarray(frame))
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=40, loop=0)


# ---------------------------------------------------------------------------
# is_youtube_url
# ---------------------------------------------------------------------------


def test_is_youtube_url_accepts_watch_url() -> None:
    from deepfake_detector.youtube import is_youtube_url

    assert is_youtube_url("https://www.youtube.com/watch?v=dQw4w9WgXcQ")
    assert is_youtube_url("https://youtube.com/watch?v=dQw4w9WgXcQ")
    assert is_youtube_url("https://youtu.be/dQw4w9WgXcQ")
    assert is_youtube_url("http://youtu.be/dQw4w9WgXcQ")


def test_is_youtube_url_rejects_non_youtube() -> None:
    from deepfake_detector.youtube import is_youtube_url

    assert not is_youtube_url("/local/file.mp4")
    assert not is_youtube_url("https://vimeo.com/123456")
    assert not is_youtube_url("not-a-url")


# ---------------------------------------------------------------------------
# CLI youtube subcommand (mocked download)
# ---------------------------------------------------------------------------


def test_cli_youtube_scans_downloaded_clip(tmp_path: Path) -> None:
    """The youtube subcommand scans the file produced by the download helper."""
    clip = tmp_path / "clip.gif"
    _write_gif(clip)

    @contextlib.contextmanager
    def fake_download(_url: str):
        yield clip

    from deepfake_detector.cli import main

    captured = StringIO()
    with patch("deepfake_detector.cli.download_youtube_clip", side_effect=fake_download), \
         patch("deepfake_detector.cli.is_youtube_url", return_value=True), \
         patch("sys.stdout", captured):
        rc = main(["youtube", "https://youtu.be/dQw4w9WgXcQ", "--json"])

    assert rc == 0, captured.getvalue()
    payload = json.loads(captured.getvalue())
    assert "authenticity_score" in payload
    assert "confidence" in payload


def test_cli_youtube_rejects_bad_url(tmp_path: Path) -> None:
    """The youtube subcommand exits with code 2 for non-YouTube URLs."""
    repo_root = Path(__file__).resolve().parents[2]
    import subprocess

    completed = subprocess.run(
        [
            sys.executable,
            "-m",
            "deepfake_detector.cli",
            "youtube",
            "not-a-youtube-url",
        ],
        check=False,
        capture_output=True,
        text=True,
        env=_pythonpath_env(repo_root),
    )

    assert completed.returncode == 2
    assert "does not look like a YouTube URL" in completed.stderr
