# Nasty-boys

Automation baseline for the repository lives in `.github/`.

## Project

- Shader module: `shader_system/` (C++17, CMake)
- Deepfake detector module: `deepfake_detector/` (Python)

## Validation

Shader system:

```bash
cmake -S shader_system -B build
cmake --build build
```

Deepfake detector:

```bash
python -m pip install -e ./deepfake_detector[dev]
python -m pytest deepfake_detector/tests -q
```

## Contribution and automation docs

- `CONTRIBUTING.md`
- `docs/automation.md`
- `docs/deepfake-detector.md`
