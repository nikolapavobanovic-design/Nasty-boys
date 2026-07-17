from __future__ import annotations

import numpy as np


def _high_frequency_ratio(frame: np.ndarray) -> float:
    gray = frame.mean(axis=2)
    centered = gray - gray.mean()
    spectrum = np.fft.fftshift(np.fft.fft2(centered))
    magnitude = np.abs(spectrum)

    h, w = magnitude.shape
    cy, cx = h // 2, w // 2
    radius = max(1, min(h, w) // 8)

    yy, xx = np.ogrid[:h, :w]
    low_freq_mask = (yy - cy) ** 2 + (xx - cx) ** 2 <= radius**2

    total_energy = float(magnitude.sum()) + 1e-8
    high_energy = float(magnitude[~low_freq_mask].sum())
    return high_energy / total_energy


def _block_boundary_energy(frame: np.ndarray, block_size: int = 8) -> float:
    gray = frame.mean(axis=2)
    if gray.shape[0] <= block_size or gray.shape[1] <= block_size:
        return 0.0

    horizontal_diff = np.abs(np.diff(gray, axis=1))
    vertical_diff = np.abs(np.diff(gray, axis=0))

    horizontal_boundaries = horizontal_diff[:, block_size - 1 :: block_size]
    vertical_boundaries = vertical_diff[block_size - 1 :: block_size, :]

    base_h = np.abs(np.diff(gray[:, : block_size * (gray.shape[1] // block_size)], axis=1)).mean() + 1e-8
    base_v = np.abs(np.diff(gray[: block_size * (gray.shape[0] // block_size), :], axis=0)).mean() + 1e-8

    boundary_mean = (horizontal_boundaries.mean() + vertical_boundaries.mean()) / 2.0
    base_mean = (base_h + base_v) / 2.0
    return float(np.clip((boundary_mean / base_mean) - 1.0, 0.0, 2.0) / 2.0)


def _channel_gradient_inconsistency(frame: np.ndarray) -> float:
    gradients = []
    for c in range(3):
        gx, gy = np.gradient(frame[:, :, c])
        gradients.append(np.sqrt(gx**2 + gy**2).ravel())

    corr_rg = np.corrcoef(gradients[0], gradients[1])[0, 1]
    corr_gb = np.corrcoef(gradients[1], gradients[2])[0, 1]
    corr_rb = np.corrcoef(gradients[0], gradients[2])[0, 1]
    mean_corr = np.nanmean([corr_rg, corr_gb, corr_rb])
    if np.isnan(mean_corr):
        return 0.5
    return float(np.clip(1.0 - mean_corr, 0.0, 1.0))


def _temporal_flicker(frames: list[np.ndarray]) -> float:
    if len(frames) < 2:
        return 0.0

    deltas = []
    for i in range(1, len(frames)):
        deltas.append(np.mean(np.abs(frames[i] - frames[i - 1])))

    mean_delta = float(np.mean(deltas))
    std_delta = float(np.std(deltas))
    if mean_delta < 1e-6:
        return 0.0

    coeff_var = std_delta / mean_delta
    return float(np.clip(coeff_var, 0.0, 1.0))


def extract_signals(frames: list[np.ndarray]) -> dict[str, float]:
    high_freq = float(np.mean([_high_frequency_ratio(frame) for frame in frames]))
    blockiness = float(np.mean([_block_boundary_energy(frame) for frame in frames]))
    channel_inconsistency = float(np.mean([_channel_gradient_inconsistency(frame) for frame in frames]))
    temporal_flicker = _temporal_flicker(frames)

    return {
        "high_frequency_ratio": high_freq,
        "blockiness": blockiness,
        "channel_inconsistency": channel_inconsistency,
        "temporal_flicker": temporal_flicker,
    }
