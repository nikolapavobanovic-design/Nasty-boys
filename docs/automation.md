# Repository automation

## Rollout increments

1. CI + PR template + CODEOWNERS
2. Security automation
3. Release automation

## Branch protection-ready checks

Configure branch protection on the default branch to require:

- `CodeQL / analyze`
- `Secret scan / gitleaks`
- `Deepfake detector CI / test-detector`

## Shader system automation status

- Shader system CI request/push workflows are disabled.
- Shader system release artifact uploads are disabled.

## Dependency and security automation

- Dependabot updates GitHub Actions dependencies.
- CodeQL scans Python.
- Secret scanning workflow runs with read-only repository permissions and the
  default `GITHUB_TOKEN`.
- Deepfake detector CI runs Python tests for `deepfake_detector/`.
