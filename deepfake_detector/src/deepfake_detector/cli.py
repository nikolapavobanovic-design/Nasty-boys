from __future__ import annotations

import argparse
import json
import logging
import sys
from dataclasses import asdict
from pathlib import Path

from .io import MediaLoadError, IMAGE_EXTENSIONS, VIDEO_EXTENSIONS
from .pipeline import scan_media

LOGGER = logging.getLogger("deepfake_detector")


def _configure_logging(debug: bool) -> None:
    logging.basicConfig(
        level=logging.DEBUG if debug else logging.INFO,
        format="%(levelname)s: %(message)s",
    )


def _print_result(result: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(result, indent=2, sort_keys=True))
        return

    print(f"Input: {result['input_path']}")
    print(f"Authenticity score: {result['authenticity_score']:.3f}")
    print(f"Confidence: {result['confidence']:.3f}")
    print(f"Likely synthetic: {result['likely_synthetic']}")
    print("Signals:")
    for name, value in sorted(result["signals"].items()):
        print(f"  - {name}: {value:.3f}")


def _collect_media_files(paths: list[str]) -> list[Path]:
    supported = IMAGE_EXTENSIONS | VIDEO_EXTENSIONS
    collected: list[Path] = []
    for raw_path in paths:
        path = Path(raw_path)
        if path.is_file() and path.suffix.lower() in supported:
            collected.append(path)
            continue
        if path.is_dir():
            for child in sorted(path.rglob("*")):
                if child.is_file() and child.suffix.lower() in supported:
                    collected.append(child)
            continue
        LOGGER.warning("Skipping unsupported path: %s", path)

    return collected


def _scan_one(input_path: str, max_frames: int, as_json: bool) -> int:
    try:
        result = asdict(scan_media(input_path, max_frames=max_frames))
    except MediaLoadError as exc:
        LOGGER.error("Failed to scan '%s': %s", input_path, exc)
        return 2

    _print_result(result, as_json=as_json)
    return 0


def _scan_batch(input_paths: list[str], max_frames: int, as_json: bool) -> int:
    files = _collect_media_files(input_paths)
    if not files:
        LOGGER.error("No supported media files found in provided paths")
        return 2

    failures = 0
    results = []
    for path in files:
        try:
            result = asdict(scan_media(str(path), max_frames=max_frames))
            results.append(result)
        except MediaLoadError as exc:
            LOGGER.error("Failed to scan '%s': %s", path, exc)
            failures += 1

    if as_json:
        print(json.dumps(results, indent=2, sort_keys=True))
    else:
        for result in results:
            _print_result(result, as_json=False)
            print()

    return 1 if failures else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Detect synthetic media signals")
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")

    subparsers = parser.add_subparsers(dest="command", required=True)

    scan = subparsers.add_parser("scan", help="Scan a single media file")
    scan.add_argument("file", help="Path to media file")
    scan.add_argument("--json", action="store_true", help="Output JSON")
    scan.add_argument("--max-frames", type=int, default=24, help="Maximum frames to inspect")

    batch = subparsers.add_parser("batch", help="Scan multiple files or directories")
    batch.add_argument("inputs", nargs="+", help="Files or directories to scan")
    batch.add_argument("--json", action="store_true", help="Output JSON")
    batch.add_argument("--max-frames", type=int, default=24, help="Maximum frames to inspect")

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    _configure_logging(args.debug)

    if args.max_frames <= 0:
        LOGGER.error("--max-frames must be > 0")
        return 2

    if args.command == "scan":
        return _scan_one(args.file, max_frames=args.max_frames, as_json=args.json)
    if args.command == "batch":
        return _scan_batch(args.inputs, max_frames=args.max_frames, as_json=args.json)

    parser.error("Unknown command")
    return 2


if __name__ == "__main__":
    sys.exit(main())
