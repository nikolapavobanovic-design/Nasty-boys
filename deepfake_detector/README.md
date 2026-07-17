# Deepfake detector (MVP)

This module provides a **defensive synthetic-media detection tool** for local analysis.
It performs heuristic signal checks on image and video inputs and reports:

- authenticity score (`0.0..1.0`)
- confidence (`0.0..1.0`)
- explanation signals:
  - high-frequency ratio
  - blockiness
  - channel inconsistency
  - temporal flicker

## Setup

```bash
python -m pip install -e ./deepfake_detector[dev]
```

## Usage

Scan one file:

```bash
deepfake-detector scan /path/to/file.png
```

JSON output:

```bash
deepfake-detector scan /path/to/file.mp4 --json
```

Batch mode over files/directories:

```bash
deepfake-detector batch /path/to/media_dir --json
```

## Debugging

Use debug logs:

```bash
deepfake-detector --debug scan /path/to/file.png
```

Common failures:
- Unsupported extension: file type is not in the supported image/video extension set.
- No frames extracted: input is corrupted or an unsupported codec is used.
- Runtime load error: dependencies (for example video codec backends) are missing.

## Limitations

- This MVP is heuristic-based and **not** a forensic-grade classifier.
- Scores are advisory and can produce false positives/negatives.
- Results should be combined with metadata checks and human review.
