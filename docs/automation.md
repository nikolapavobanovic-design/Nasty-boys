# Repository automation

## Rollout increments

1. CI + PR template + CODEOWNERS
2. Security automation
3. Release automation

## Branch protection-ready checks

Configure branch protection on the default branch to require:

- `PR CI / build-shader-system`
- `CodeQL / analyze`
- `Secret scan / gitleaks`
- `Deepfake detector CI / test-detector`

## CI entrypoint

All CI build jobs use:

```bash
cmake -S shader_system -B build
cmake --build build
```

## Failure diagnostics

CI uploads build/configure logs as workflow artifacts and writes a failure summary
to the workflow job summary when a build step fails.

## Dependency and security automation

- Dependabot updates GitHub Actions dependencies.
- CodeQL scans C/C++.
- Secret scanning workflow runs with read-only repository permissions and the
  default `GITHUB_TOKEN`.
- Deepfake detector CI runs Python tests for `deepfake_detector/`.

## Release automation

- Tag format: `vMAJOR.MINOR.PATCH`
- Tag push triggers build + GitHub Release publication.
- Release notes are generated automatically from merged PR metadata.
