# Deepfake detector: setup and operations

## Goal

Provide a defensive workflow to inspect media for synthetic-generation signals.

## Local setup

```bash
python -m pip install -e ./deepfake_detector[dev]
```

## Run detector

```bash
deepfake-detector scan /path/to/media.png
deepfake-detector batch /path/to/media_dir --json
```

## Test and debug

```bash
python -m pytest deepfake_detector/tests -q
```

Enable debug logging when reproducing failures:

```bash
deepfake-detector --debug scan /path/to/file.png
```

## CI

The repository includes a dedicated workflow that runs detector tests on PR/push for
changes under `deepfake_detector/**`, `docs/**`, and `.github/workflows/**`.

## Output interpretation

- `authenticity_score`: Higher means more likely authentic.
- `confidence`: Higher means stronger separation from uncertainty.
- `signals`: Individual heuristic components useful for triage.

## Safety and review

- Treat all output as probabilistic.
- Require human review for high-impact decisions.
- Do not use this detector as the sole basis for punitive or legal decisions.
