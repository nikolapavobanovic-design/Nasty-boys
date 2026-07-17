# Contributing

## Scope

Repository automation currently covers:
- `shader_system/`
- `deepfake_detector/`

## Local validation

Run this before opening a PR:

```bash
cmake -S shader_system -B build
cmake --build build
python -m pip install -e ./deepfake_detector[dev]
python -m pytest deepfake_detector/tests -q
```

## Commit and PR expectations

- Keep changes focused and minimal.
- Use clear commit messages describing the behavior change.
- Fill out the PR template completely, including validation steps and risk.
- Ensure required CI checks are green before merge.

## Versioning

- Tags follow SemVer: `vMAJOR.MINOR.PATCH` (for example `v1.2.0`).
- Releases are created from tags only.
