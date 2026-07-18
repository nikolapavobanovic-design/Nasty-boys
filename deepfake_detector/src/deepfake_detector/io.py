from __future__ import annotations

from pathlib import Path

import imageio.v3 as iio
import numpy as np
from PIL import Image

IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".webp"}
VIDEO_EXTENSIONS = {".mp4", ".mov", ".avi", ".mkv", ".webm", ".gif"}


class MediaLoadError(RuntimeError):
    """Raised when media loading fails."""


def _normalize_frame(frame: np.ndarray) -> np.ndarray:
    if frame.ndim == 2:
        frame = np.stack([frame] * 3, axis=-1)
    if frame.ndim != 3:
        raise MediaLoadError(f"Unsupported frame dimensions: {frame.ndim}")
    if frame.shape[2] == 4:
        frame = frame[..., :3]
    if frame.shape[2] != 3:
        raise MediaLoadError(f"Unsupported channel count: {frame.shape[2]}")

    frame = frame.astype(np.float32)
    if frame.max() > 1.0:
        frame = frame / 255.0
    return np.clip(frame, 0.0, 1.0)


def load_media_frames(path: str | Path, max_frames: int = 24) -> list[np.ndarray]:
    source = Path(path)
    if not source.exists() or not source.is_file():
        raise MediaLoadError(f"Input path does not exist or is not a file: {source}")

    suffix = source.suffix.lower()
    if suffix in IMAGE_EXTENSIONS:
        try:
            img = Image.open(source).convert("RGB")
            return [_normalize_frame(np.asarray(img))]
        except Exception as exc:  # pragma: no cover - passthrough
            raise MediaLoadError(f"Failed to read image '{source}': {exc}") from exc

    if suffix in VIDEO_EXTENSIONS:
        try:
            frames: list[np.ndarray] = []
            for idx, frame in enumerate(iio.imiter(source)):
                if idx >= max_frames:
                    break
                frames.append(_normalize_frame(np.asarray(frame)))

            if not frames:
                raise MediaLoadError(f"No frames could be extracted from '{source}'")
            return frames
        except MediaLoadError:
            raise
        except Exception as exc:  # pragma: no cover - passthrough
            raise MediaLoadError(f"Failed to read video '{source}': {exc}") from exc

    raise MediaLoadError(
        f"Unsupported media extension '{suffix}'. "
        f"Supported images: {sorted(IMAGE_EXTENSIONS)}; videos: {sorted(VIDEO_EXTENSIONS)}"
    )
