"""YouTube URL download helper.

Requires the optional ``youtube`` extra::

    pip install 'deepfake-detector[youtube]'
"""
from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path
from typing import Generator

# Matches http(s)://[www.]youtube.com/watch?...v=ID and youtu.be/ID
_YOUTUBE_RE = re.compile(
    r"^https?://(www\.)?(youtube\.com/watch\?[^\s]*v=[\w-]+|youtu\.be/[\w-]+)",
    re.IGNORECASE,
)


def is_youtube_url(text: str) -> bool:
    """Return True if *text* looks like a YouTube watch URL."""
    return bool(_YOUTUBE_RE.match(text.strip()))


def _require_yt_dlp() -> None:
    import importlib.util

    if importlib.util.find_spec("yt_dlp") is None:
        raise RuntimeError(
            "yt-dlp is required for YouTube scanning. "
            "Install it with: pip install 'deepfake-detector[youtube]'"
        )


@contextmanager
def download_youtube_clip(url: str) -> Generator[Path, None, None]:
    """Download a YouTube clip to a temporary file and yield its :class:`Path`.

    The temporary directory (and the downloaded file) is removed when the
    context manager exits.

    Raises:
        RuntimeError: if yt-dlp is not installed or the download fails.
    """
    _require_yt_dlp()

    with tempfile.TemporaryDirectory(prefix="dd_yt_") as tmp_dir:
        out_template = str(Path(tmp_dir) / "clip.%(ext)s")
        cmd = [
            sys.executable,
            "-m",
            "yt_dlp",
            "--no-playlist",
            "--max-filesize",
            "200M",
            "-f",
            "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best",
            "-o",
            out_template,
            url,
        ]
        result = subprocess.run(cmd, check=False, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(
                f"yt-dlp failed to download '{url}':\n{result.stderr.strip()}"
            )

        files = sorted(Path(tmp_dir).iterdir())
        if not files:
            raise RuntimeError(f"yt-dlp produced no output files for '{url}'")

        yield files[0]
