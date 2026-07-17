# Contributing

## Scope

All repository automation is scoped to the shader module at:
`shader_system/`

## Local validation

Run this before opening a PR:

```bash
cmake -S shader_system -B build
cmake --build build
```

## Commit and PR expectations

- Keep changes focused and minimal.
- Use clear commit messages describing the behavior change.
- Fill out the PR template completely, including validation steps and risk.
- Ensure required CI checks are green before merge.

## Versioning

- Tags follow SemVer: `vMAJOR.MINOR.PATCH` (for example `v1.2.0`).
- Releases are created from tags only.
