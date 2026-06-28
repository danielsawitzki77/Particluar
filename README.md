# Particluar

A particle-based game built in C++ with SDL3.

## Getting Started

### Build

Open `Particluar.sln` in Visual Studio and build (Debug or Release, x64).

### Git Hooks

Install the pre-push build verification hook:

```bash
git config core.hooksPath .githooks
```

This blocks pushes when the build fails for either Debug or Release configuration. Bypass with `git push --no-verify` for exceptional cases only.

## Vendor Dependencies

Vendor dependencies (SDL, SDL_image, picojson, stb) are updated using the centralized script in Game-Dev-Supreme:

```batch
..\Game-Dev-Supreme\update_vendor.bat
```

Run this from the project root. It auto-detects the calling directory and updates `vendor\` from sibling source repos.

## Development Workflow

- Feature branches: `issue-<number>-<short-description>`
- Conventional commits: `feat:`, `fix:`, `refactor:`, etc.
- PRs reference the issue they address
