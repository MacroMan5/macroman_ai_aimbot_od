# Git Hooks - Local Test Enforcement

This directory contains Git hooks that enforce comprehensive testing **locally** before commits.

## Why Local Hooks?

**CI/CD Strategy:**
- **CI (GitHub Actions):** Runs **unit tests only** for fast feedback (~2-3 min)
- **Local Hooks:** Runs **full test suite** (unit + integration + benchmarks) before commits

**Rationale:**
- Integration tests require specific GPU/timing conditions that CI runners may not provide reliably
- Benchmark tests need consistent hardware for meaningful results
- Local enforcement ensures code quality without slowing down CI feedback

## Installation

### Automatic Setup (Recommended)

Run the setup script from the repository root:

**Windows (PowerShell):**
```powershell
.\scripts\setup-hooks.ps1
```

**Linux/macOS:**
```bash
chmod +x scripts/setup-hooks.sh
./scripts/setup-hooks.sh
```

### Manual Setup

Copy the hooks to `.git/hooks/`:

**Windows (PowerShell):**
```powershell
Copy-Item .githooks\pre-commit .git\hooks\pre-commit -Force
```

**Linux/macOS:**
```bash
cp .githooks/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

## Hooks Overview

### `pre-commit`

**Purpose:** Run full test suite before each commit

**What it does:**
1. Detects build directory (`build/` or `build-verify/`)
2. Runs unit tests (`ctest -L unit`)
3. Runs integration tests (`ctest -L integration`)
4. Validates benchmark executable (smoke test)

**Execution time:** ~30-60 seconds (depends on test suite size)

**Skip the hook (emergencies only):**
```bash
git commit --no-verify -m "Emergency fix"
```

## Requirements

Before using the hooks, ensure you have:
1. **Built the project:** `cmake -B build -S . && cmake --build build --config Release`
2. **Test labels configured:** CMakeLists.txt files should have `LABELS "unit"` and `LABELS "integration"`

## Troubleshooting

### Hook not running
- Verify the hook file is in `.git/hooks/` (not `.githooks/`)
- On Linux/macOS: Ensure the hook is executable (`chmod +x .git/hooks/pre-commit`)

### "No build directory found"
- Build the project first: `cmake -B build -S . && cmake --build build --config Release`

### Tests failing
- Fix the failing tests before committing
- Run tests manually to debug: `ctest --test-dir build -C Release -L unit --output-on-failure`

### Need to commit urgently
- Use `git commit --no-verify` to skip hooks (NOT RECOMMENDED for regular use)

## CI/CD Integration

**GitHub Actions Workflow (.github/workflows/ci.yml):**
- Only runs **unit tests** with `ctest -L unit`
- Skips integration tests and benchmarks for speed
- See workflow file for details

**Local Pre-Commit Hook:**
- Runs **full test suite** (unit + integration + benchmark smoke test)
- Ensures code quality before push

This two-tier strategy provides:
- ✅ Fast CI feedback (2-3 minutes)
- ✅ Comprehensive local testing (30-60 seconds)
- ✅ No flaky GPU-dependent tests in CI

## Maintenance

When adding new test categories:
1. Add label in CMakeLists.txt: `catch_discover_tests(... PROPERTIES LABELS "new_category")`
2. Update `.githooks/pre-commit` to run the new tests
3. Update this README

---

**Last Updated:** 2026-01-03
**Managed By:** MacroMan AI Aimbot Development Team
