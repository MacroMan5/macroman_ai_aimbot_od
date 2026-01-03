# Code Coverage - MacroMan AI Aimbot

This document explains how to generate and view code coverage reports for the MacroMan AI Aimbot project.

---

## Overview

Code coverage measures which parts of your codebase are executed during testing. This helps identify:
- **Untested code**: Functions or branches not covered by tests
- **Test quality**: How thoroughly your tests exercise the code
- **Regression risks**: Areas lacking test coverage that may break during changes

**Coverage targets for this project:**
- Algorithms (Kalman, NMS, Bezier): 100%
- Core utilities: 80%
- Interfaces: Compile-time only (no runtime coverage needed)

---

## Prerequisites

### Windows (MSVC)

Download and install **OpenCppCoverage**:
1. Go to: https://github.com/OpenCppCoverage/OpenCppCoverage/releases
2. Download latest installer (e.g., `OpenCppCoverageSetup-x64-0.9.9.0.exe`)
3. Run installer (install to default location: `C:\Program Files\OpenCppCoverage`)

### Linux/macOS (GCC/Clang)

Install lcov:
```bash
# Ubuntu/Debian
sudo apt-get install lcov

# macOS
brew install lcov
```

---

## Building with Coverage

### Configure CMake with Coverage Enabled

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON
```

**Note:** Use `Debug` build for accurate line coverage. `Release` builds may optimize away code.

### Build Tests

```bash
cmake --build build --config Debug -j
```

---

## Generating Coverage Reports

### Option 1: Run Coverage Targets (Recommended)

CMake creates coverage targets automatically:

```bash
# Unit test coverage
cmake --build build --target coverage_unit_tests

# Integration test coverage
cmake --build build --target coverage_integration_tests

# View reports (Windows)
start build/coverage/unit/index.html
start build/coverage/integration/index.html

# View reports (Linux/macOS)
open build/coverage/unit/index.html
open build/coverage/integration/index.html
```

### Option 2: Manual OpenCppCoverage

```powershell
# Run unit tests with coverage
OpenCppCoverage.exe `
    --sources src `
    --export_type html:coverage/unit `
    --export_type cobertura:coverage/unit/coverage.xml `
    --excluded_sources build `
    --excluded_sources external `
    -- build/bin/Debug/unit_tests.exe

# Open report
start coverage/unit/index.html
```

---

## CI/CD Integration (GitHub Actions)

The CI/CD pipeline automatically generates coverage reports on pull requests.

See `.github/workflows/ci.yml` for implementation:

```yaml
- name: Run tests with coverage
  run: |
    cmake --build build --target coverage_unit_tests
    cmake --build build --target coverage_integration_tests

- name: Upload coverage reports
  uses: codecov/codecov-action@v4
  with:
    files: ./build/coverage/*/coverage.xml
    flags: unittests
    name: codecov-umbrella
```

---

## Understanding Coverage Reports

### HTML Report (Interactive)

Open `build/coverage/unit/index.html` in a browser to see:
- **Overall coverage**: Percentage of lines executed
- **File-by-file breakdown**: Click files to see line-by-line coverage
- **Color coding**:
  - **Green**: Line executed by tests
  - **Red**: Line NOT executed (needs more tests)
  - **Orange**: Partially covered (e.g., only some branches of an if/else)

### XML Report (Machine-Readable)

Located at `build/coverage/unit/coverage.xml` (Cobertura format).

Used for:
- CI/CD integration (e.g., Codecov, Coveralls)
- Automated coverage threshold checks
- Trend analysis over time

---

## Coverage Exclusions

Some code is intentionally excluded from coverage:

### Automatic Exclusions (via CMake)
- `build/` directory (generated code)
- `external/` directory (third-party dependencies)
- `modules/` directory (pre-compiled SDKs)

### Line-Level Exclusions
- Lines with `assert()` (debugging only, removed in Release)
- Lines with `LOG_*` macros (logging)

To exclude specific lines manually:
```cpp
// LCOV_EXCL_START
void platformSpecificDebugCode() {
    // This won't count against coverage
}
// LCOV_EXCL_STOP
```

---

## Troubleshooting

### Issue: OpenCppCoverage not found

**Error:**
```
CMake Warning: OpenCppCoverage not found. Coverage reports will not be generated.
```

**Solution:**
1. Install OpenCppCoverage from: https://github.com/OpenCppCoverage/OpenCppCoverage/releases
2. Add installation directory to PATH (e.g., `C:\Program Files\OpenCppCoverage`)
3. Re-run CMake configuration: `cmake -B build -S . -DENABLE_COVERAGE=ON`

### Issue: No coverage data generated

**Possible causes:**
1. **Tests not running**: Check if tests execute successfully
2. **Wrong build type**: Use `Debug` build for coverage (not `Release`)
3. **Source path mismatch**: Ensure `--sources` points to correct directory

**Debug steps:**
```bash
# Verify tests run
./build/bin/Debug/unit_tests.exe

# Check OpenCppCoverage version
OpenCppCoverage.exe --help

# Run with verbose output
OpenCppCoverage.exe --verbose --sources src -- build/bin/Debug/unit_tests.exe
```

### Issue: Coverage report shows 0% for all files

**Solution:**
- Ensure you're running Debug build (not Release)
- Check that source paths match: CMake uses absolute paths, OpenCppCoverage needs matching paths
- Verify tests actually import and execute the source code

---

## Best Practices

### 1. Run Coverage Locally Before PR

```bash
# Quick coverage check
cmake --build build --target coverage_unit_tests
start build/coverage/unit/index.html

# Check for red lines (uncovered code)
# Add tests to cover critical paths
```

### 2. Focus on Critical Code

**High priority for 100% coverage:**
- Core algorithms (Kalman filtering, NMS post-processing, Bezier curves)
- Safety mechanisms (deadman switch, prediction clamping)
- Data structures (TargetDatabase, DetectionBatch)

**Lower priority:**
- UI rendering code (hard to test, low risk)
- Platform-specific error handling (requires mocking OS calls)
- Debug-only code paths

### 3. Don't Game the Metrics

**Bad practice:**
```cpp
// Fake test that adds coverage but doesn't verify correctness
TEST_CASE("Kalman filter") {
    KalmanPredictor predictor;
    predictor.predict(0.1f);  // ❌ No assertions!
}
```

**Good practice:**
```cpp
TEST_CASE("Kalman filter converges to constant velocity") {
    KalmanPredictor predictor;

    // Feed 10 samples at constant velocity
    for (int i = 0; i < 10; ++i) {
        predictor.update({100.0f + i * 10.0f, 200.0f}, 0.016f);
    }

    // Verify predicted position matches expected trajectory
    auto predicted = predictor.predict(0.048f);  // 3 frames ahead
    REQUIRE_THAT(predicted.x, WithinRel(130.0f, 0.05f));
    REQUIRE_THAT(predicted.y, WithinRel(200.0f, 0.05f));
}
```

### 4. Coverage ≠ Quality

- **100% coverage doesn't guarantee bug-free code**
- **Focus on meaningful test cases**, not just hitting every line
- **Use coverage to find gaps**, not as the only metric of test quality

---

## References

- [OpenCppCoverage Documentation](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki)
- [lcov/genhtml Manual](https://linux.die.net/man/1/lcov)
- [Codecov CI Integration](https://docs.codecov.com/docs/quick-start)
- [Code Coverage Best Practices (Microsoft)](https://learn.microsoft.com/en-us/visualstudio/test/using-code-coverage-to-determine-how-much-code-is-being-tested)

---

**Last updated:** 2025-12-31
**Maintained by:** MacroMan Development Team
