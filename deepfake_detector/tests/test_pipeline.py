from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

from deepfake_detector.io import MediaLoadError, load_media_frames
from deepfake_detector.pipeline import scan_media


def _write_real_like_image(path: Path) -> None:
    rng = np.random.default_rng(seed=42)
    x = np.linspace(0, 1, 96, dtype=np.float32)
    y = np.linspace(0, 1, 96, dtype=np.float32)
    xx, yy = np.meshgrid(x, y)
    luminance = 0.2 + 0.6 * (0.7 * xx + 0.3 * yy)
    noise = rng.normal(0.0, 0.02, size=(96, 96)).astype(np.float32)
    channel_a = np.clip(luminance + noise, 0, 1)
    channel_b = np.clip(luminance * 0.95 + noise, 0, 1)
    channel_c = np.clip(luminance * 0.9 + noise, 0, 1)
    base = np.stack([channel_a, channel_b, channel_c], axis=-1)
    Image.fromarray((base * 255).astype(np.uint8)).save(path)


def _write_synthetic_like_image(path: Path) -> None:
    grid = np.zeros((96, 96, 3), dtype=np.float32)
    for y in range(0, 96, 8):
        for x in range(0, 96, 8):
            val = 0.15 if ((x // 8 + y // 8) % 2 == 0) else 0.85
            grid[y : y + 8, x : x + 8, :] = val
    Image.fromarray((grid * 255).astype(np.uint8)).save(path)


def _write_gif(path: Path) -> None:
    frames = []
    for i in range(5):
        frame = np.zeros((64, 64, 3), dtype=np.uint8)
        frame[:, :, 0] = (i * 50) % 255
        frame[:, :, 1] = np.linspace(0, 255, 64, dtype=np.uint8)[None, :]
        frame[:, :, 2] = np.linspace(0, 255, 64, dtype=np.uint8)[:, None]
        frames.append(Image.fromarray(frame))

    frames[0].save(path, save_all=True, append_images=frames[1:], duration=40, loop=0)


def test_scan_scores_synthetic_lower_than_real(tmp_path: Path) -> None:
    real = tmp_path / "real_like.png"
    synthetic = tmp_path / "synthetic_like.png"
    _write_real_like_image(real)
    _write_synthetic_like_image(synthetic)

    real_result = scan_media(real)
    synthetic_result = scan_media(synthetic)

    assert 0.0 <= real_result.authenticity_score <= 1.0
    assert 0.0 <= synthetic_result.authenticity_score <= 1.0
    assert synthetic_result.authenticity_score < real_result.authenticity_score


def test_load_media_frames_extracts_video_frames_from_gif(tmp_path: Path) -> None:
    gif_path = tmp_path / "sample.gif"
    _write_gif(gif_path)

    frames = load_media_frames(gif_path, max_frames=3)

    assert len(frames) == 3
    assert frames[0].shape == (64, 64, 3)


def test_scan_media_raises_on_invalid_input(tmp_path: Path) -> None:
    bad_file = tmp_path / "broken.txt"
    bad_file.write_text("not media", encoding="utf-8")

    try:
        scan_media(bad_file)
    except MediaLoadError as exc:
        assert "Unsupported media extension" in str(exc)
    else:
        raise AssertionError("Expected MediaLoadError for unsupported extension")
