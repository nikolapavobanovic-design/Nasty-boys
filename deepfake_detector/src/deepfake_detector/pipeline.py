from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np

from .features import extract_signals
from .io import MediaLoadError, load_media_frames


@dataclass(slots=True)
class DetectionResult:
    input_path: str
    authenticity_score: float
    confidence: float
    likely_synthetic: bool
    signals: dict[str, float]


def _suspicion_score(signals: dict[str, float]) -> float:
    normalized_high_freq = np.clip((signals["high_frequency_ratio"] - 0.6) / 0.4, 0.0, 1.0)

    weighted = (
        0.35 * normalized_high_freq
        + 0.30 * signals["blockiness"]
        + 0.20 * signals["channel_inconsistency"]
        + 0.15 * signals["temporal_flicker"]
    )
    return float(np.clip(weighted, 0.0, 1.0))


def _confidence_from_score(authenticity_score: float, frame_count: int) -> float:
    margin = abs(authenticity_score - 0.5) * 2.0
    frame_bonus = min(frame_count / 12.0, 1.0)
    confidence = 0.55 * margin + 0.45 * frame_bonus
    return float(np.clip(confidence, 0.0, 1.0))


def scan_media(
    path: str | Path,
    max_frames: int = 24,
    threshold: float = 0.5,
) -> DetectionResult:
    input_path = str(path)
    frames = load_media_frames(input_path, max_frames=max_frames)
    if not frames:
        raise MediaLoadError(f"No analyzable frames found in '{input_path}'")

    signals = extract_signals(frames)
    suspicion = _suspicion_score(signals)
    authenticity_score = float(np.clip(1.0 - suspicion, 0.0, 1.0))
    confidence = _confidence_from_score(authenticity_score, len(frames))

    return DetectionResult(
        input_path=input_path,
        authenticity_score=authenticity_score,
        confidence=confidence,
        likely_synthetic=authenticity_score < threshold,
        signals=signals,
    )
