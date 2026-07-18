"""Tests for deepfake_detector.features and deepfake_detector.io."""
from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest
from PIL import Image

from deepfake_detector.features import (
    _block_boundary_energy,
    _channel_gradient_inconsistency,
    _high_frequency_ratio,
    _temporal_flicker,
    extract_signals,
)
from deepfake_detector.io import (
    IMAGE_EXTENSIONS,
    VIDEO_EXTENSIONS,
    MediaLoadError,
    _normalize_frame,
    load_media_frames,
)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

_RNG = np.random.default_rng(seed=0)


def _random_frame(h: int = 64, w: int = 64) -> np.ndarray:
    """Return a random float32 RGB frame in [0, 1]."""
    return _RNG.random((h, w, 3)).astype(np.float32)


def _write_png(path: Path, h: int = 48, w: int = 48) -> None:
    arr = _RNG.integers(0, 256, size=(h, w, 3), dtype=np.uint8)
    Image.fromarray(arr).save(path)


def _write_gif(path: Path, n_frames: int = 4, h: int = 32, w: int = 32) -> None:
    frames = []
    for i in range(n_frames):
        data = np.zeros((h, w, 3), dtype=np.uint8)
        data[:, :, 0] = (i * 60) % 256
        frames.append(Image.fromarray(data))
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=40, loop=0)


# ---------------------------------------------------------------------------
# io._normalize_frame
# ---------------------------------------------------------------------------


def test_normalize_frame_grayscale_becomes_rgb() -> None:
    gray = np.zeros((32, 32), dtype=np.uint8)
    result = _normalize_frame(gray)
    assert result.shape == (32, 32, 3)


def test_normalize_frame_rgba_strips_alpha() -> None:
    rgba = np.zeros((32, 32, 4), dtype=np.uint8)
    result = _normalize_frame(rgba)
    assert result.shape == (32, 32, 3)


def test_normalize_frame_uint8_rescales_to_01() -> None:
    data = np.full((16, 16, 3), 255, dtype=np.uint8)
    result = _normalize_frame(data)
    assert result.max() <= 1.0
    assert result.dtype == np.float32


def test_normalize_frame_already_float_unchanged() -> None:
    data = np.full((16, 16, 3), 0.5, dtype=np.float32)
    result = _normalize_frame(data)
    assert float(result.max()) <= 1.0
    np.testing.assert_allclose(result, data)


def test_normalize_frame_unsupported_ndim_raises() -> None:
    bad = np.zeros((16, 16, 3, 2), dtype=np.uint8)
    with pytest.raises(MediaLoadError, match="Unsupported frame dimensions"):
        _normalize_frame(bad)


def test_normalize_frame_unsupported_channels_raises() -> None:
    bad = np.zeros((16, 16, 2), dtype=np.uint8)
    with pytest.raises(MediaLoadError, match="Unsupported channel count"):
        _normalize_frame(bad)


# ---------------------------------------------------------------------------
# io.load_media_frames
# ---------------------------------------------------------------------------


def test_load_media_frames_missing_file_raises(tmp_path: Path) -> None:
    with pytest.raises(MediaLoadError, match="does not exist"):
        load_media_frames(tmp_path / "no_such_file.png")


def test_load_media_frames_unsupported_extension_raises(tmp_path: Path) -> None:
    bad = tmp_path / "file.xyz"
    bad.write_text("not media", encoding="utf-8")
    with pytest.raises(MediaLoadError, match="Unsupported media extension"):
        load_media_frames(bad)


def test_load_media_frames_image_returns_single_frame(tmp_path: Path) -> None:
    img_path = tmp_path / "img.png"
    _write_png(img_path)
    frames = load_media_frames(img_path)
    assert len(frames) == 1
    assert frames[0].ndim == 3
    assert frames[0].shape[2] == 3
    assert frames[0].dtype == np.float32
    assert frames[0].min() >= 0.0
    assert frames[0].max() <= 1.0


def test_load_media_frames_gif_respects_max_frames(tmp_path: Path) -> None:
    gif_path = tmp_path / "clip.gif"
    _write_gif(gif_path, n_frames=6)
    frames = load_media_frames(gif_path, max_frames=3)
    assert len(frames) == 3


def test_load_media_frames_gif_all_frames(tmp_path: Path) -> None:
    gif_path = tmp_path / "clip.gif"
    _write_gif(gif_path, n_frames=4)
    frames = load_media_frames(gif_path, max_frames=100)
    assert len(frames) == 4


def test_image_and_video_extension_sets_non_overlapping() -> None:
    assert IMAGE_EXTENSIONS.isdisjoint(VIDEO_EXTENSIONS)


# ---------------------------------------------------------------------------
# features._high_frequency_ratio
# ---------------------------------------------------------------------------


def test_high_frequency_ratio_returns_value_in_range() -> None:
    frame = _random_frame()
    ratio = _high_frequency_ratio(frame)
    assert 0.0 <= ratio <= 1.0


def test_high_frequency_ratio_uniform_frame_low() -> None:
    uniform = np.full((64, 64, 3), 0.5, dtype=np.float32)
    ratio = _high_frequency_ratio(uniform)
    assert ratio < 0.5, "Uniform image should have low high-frequency energy"


# ---------------------------------------------------------------------------
# features._block_boundary_energy
# ---------------------------------------------------------------------------


def test_block_boundary_energy_returns_value_in_range() -> None:
    frame = _random_frame()
    energy = _block_boundary_energy(frame)
    assert 0.0 <= energy <= 1.0


def test_block_boundary_energy_tiny_frame_returns_zero() -> None:
    tiny = np.zeros((4, 4, 3), dtype=np.float32)
    assert _block_boundary_energy(tiny) == 0.0


def test_block_boundary_energy_checkerboard_nonzero() -> None:
    grid = np.zeros((64, 64, 3), dtype=np.float32)
    for y in range(0, 64, 8):
        for x in range(0, 64, 8):
            val = 0.1 if ((x // 8 + y // 8) % 2 == 0) else 0.9
            grid[y : y + 8, x : x + 8, :] = val
    energy = _block_boundary_energy(grid)
    assert energy > 0.0


# ---------------------------------------------------------------------------
# features._channel_gradient_inconsistency
# ---------------------------------------------------------------------------


def test_channel_gradient_inconsistency_returns_value_in_range() -> None:
    frame = _random_frame()
    val = _channel_gradient_inconsistency(frame)
    assert 0.0 <= val <= 1.0


def test_channel_gradient_inconsistency_uniform_returns_half() -> None:
    uniform = np.full((32, 32, 3), 0.5, dtype=np.float32)
    val = _channel_gradient_inconsistency(uniform)
    # Gradients are all zero → corrcoef is nan → returns 0.5
    assert val == pytest.approx(0.5)


def test_channel_gradient_inconsistency_identical_channels_near_zero() -> None:
    base = _RNG.random((32, 32)).astype(np.float32)
    identical = np.stack([base, base, base], axis=-1)
    val = _channel_gradient_inconsistency(identical)
    assert val == pytest.approx(0.0, abs=1e-5)


# ---------------------------------------------------------------------------
# features._temporal_flicker
# ---------------------------------------------------------------------------


def test_temporal_flicker_single_frame_returns_zero() -> None:
    assert _temporal_flicker([_random_frame()]) == 0.0


def test_temporal_flicker_empty_returns_zero() -> None:
    assert _temporal_flicker([]) == 0.0


def test_temporal_flicker_identical_frames_returns_zero() -> None:
    frame = _random_frame()
    assert _temporal_flicker([frame, frame, frame]) == 0.0


def test_temporal_flicker_varying_frames_returns_value_in_range() -> None:
    frames = [_random_frame() for _ in range(5)]
    val = _temporal_flicker(frames)
    assert 0.0 <= val <= 1.0


# ---------------------------------------------------------------------------
# features.extract_signals
# ---------------------------------------------------------------------------


def test_extract_signals_returns_expected_keys() -> None:
    frames = [_random_frame() for _ in range(3)]
    signals = extract_signals(frames)
    assert set(signals) == {
        "high_frequency_ratio",
        "blockiness",
        "channel_inconsistency",
        "temporal_flicker",
    }


def test_extract_signals_all_values_in_range() -> None:
    frames = [_random_frame() for _ in range(4)]
    signals = extract_signals(frames)
    for name, value in signals.items():
        assert 0.0 <= value <= 1.0, f"Signal '{name}' out of range: {value}"
