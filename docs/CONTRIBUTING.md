# Contributing Guide

Welcome to the MacroMan AI Aimbot project! This guide will help you contribute high-quality code that follows our standards.

---

## Code Quality Standards

We use automated tools to ensure consistent code quality across the project:

- **clang-format**: Automatic code formatting (LLVM style)
- **clang-tidy**: Static analysis to catch bugs and enforce best practices
- **Catch2**: Unit testing framework

---

## Running clang-format Locally

### Check Formatting (Dry-Run)

Check if your code follows the project's formatting standards **without making changes**:

```bash
# Check all C++ files
clang-format -style=file --dry-run --Werror src/**/*.cpp src/**/*.h

# Check specific file
clang-format -style=file --dry-run --Werror src/core/threading/LatestFrameQueue.h
```

### Apply Formatting

Automatically format your code to match the project standards:

```bash
# Format all C++ files
clang-format -style=file -i src/**/*.cpp src/**/*.h

# Format specific file
clang-format -style=file -i src/core/threading/LatestFrameQueue.h
```

**Pro Tip**: Configure your IDE to format on save using `.clang-format`:
- **VSCode**: Install "C/C++" extension, enable "Format On Save"
- **Visual Studio**: Tools → Options → Text Editor → C/C++ → Code Style → Formatting → Enable "ClangFormat"

---

## Running clang-tidy Locally

### Generate compile_commands.json

clang-tidy requires `compile_commands.json` to understand your project structure:

```bash
cmake -B build -S . --preset=x64-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### Run clang-tidy

```bash
# Check specific file
clang-tidy -p build src/core/threading/LatestFrameQueue.h

# Check all files in a directory
clang-tidy -p build src/core/**/*.cpp
```

### Fix Issues Automatically

Some clang-tidy warnings can be auto-fixed:

```bash
clang-tidy -p build --fix src/core/threading/LatestFrameQueue.h
```

**⚠️ Warning**: Review auto-fixes before committing!

---

## Code Style Guidelines

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| **Classes/Structs** | PascalCase | `TargetDatabase`, `DMLDetector` |
| **Functions/Methods** | camelCase | `captureFrame()`, `loadModel()` |
| **Variables** | camelCase | `frameCount`, `detectionResults` |
| **Constants** | UPPER_SNAKE_CASE | `MAX_TARGETS`, `DEFAULT_FOV` |
| **Interfaces** | Prefix with `I` | `IDetector`, `IScreenCapture` |
| **Namespaces** | lower_case | `macroman`, `core` |

### Formatting Rules

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Max 100 characters
- **Braces**: K&R style (opening brace on same line)
  ```cpp
  void function() {
      // K&R style
  }
  ```
- **Pointer Alignment**: Left-aligned
  ```cpp
  int* ptr;        // ✅ Correct
  int *ptr;        // ❌ Wrong
  int * ptr;       // ❌ Wrong
  ```

### Include Order

```cpp
// 1. Project headers (quotes)
#include "core/types/Vec2.h"
#include "detection/DMLDetector.h"

// 2. C++ standard library
#include <memory>
#include <vector>

// 3. System/third-party headers
#include <spdlog/spdlog.h>
#include <Eigen/Dense>
```

### Modern C++ Best Practices

- ✅ Use `nullptr` instead of `NULL`
- ✅ Use `auto` for obvious types
- ✅ Use `override` for virtual methods
- ✅ Use `std::unique_ptr`, `std::shared_ptr` (never raw `new`/`delete`)
- ✅ Use `constexpr` for compile-time constants
- ❌ Avoid C-style casts (use `static_cast`, `dynamic_cast`)
- ❌ Avoid macros (use `constexpr` or inline functions)

---

## Testing

### Writing Unit Tests

All new features and bug fixes should include unit tests using Catch2:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("MyFeature works correctly", "[myfeature]") {
    SECTION("Basic functionality") {
        // Arrange
        MyClass obj;

        // Act
        auto result = obj.doSomething();

        // Assert
        REQUIRE(result == expectedValue);
    }
}
```

### Running Tests Locally

```bash
# Build tests
cmake --build build --config Release --target unit_tests

# Run all tests
ctest --test-dir build -C Release --output-on-failure

# Run specific test
./build/bin/Release/unit_tests.exe "[myfeature]"
```

---

## CI/CD Workflow

Our CI pipeline automatically checks:

1. **Code Quality** (30-60 seconds)
   - clang-format validation
   - clang-tidy static analysis
   - Auto-suggests fixes in PR comments

2. **Documentation** (10-20 seconds)
   - Checks for legacy references

3. **Build & Test** (2-4 minutes)
   - Windows MSVC 2022 build
   - Unit tests (Catch2)
   - Benchmark smoke test

**Total Pipeline Time**: ~2-4 minutes

### Pull Request Workflow

1. **Create Feature Branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make Changes** (format code before committing):
   ```bash
   clang-format -style=file -i src/**/*.cpp src/**/*.h
   ```

3. **Commit Changes**:
   ```bash
   git add .
   git commit -m "feat: add new feature"
   ```

4. **Push and Create PR**:
   ```bash
   git push origin feature/your-feature-name
   # Open PR on GitHub
   ```

5. **Review CI Feedback**:
   - Check CI status badge in PR
   - Review clang-tidy comments (auto-fix suggestions)
   - Fix any failing checks locally

6. **Merge** (after CI passes):
   - Maintainer will merge your PR

---

## Common Issues & Solutions

### Issue: clang-format not found

**Solution**: Install LLVM toolchain:
- **Windows**: [Download LLVM](https://github.com/llvm/llvm-project/releases)
- **Linux**: `sudo apt install clang-format`
- **macOS**: `brew install clang-format`

### Issue: clang-tidy reports false positives

**Solution**: Add `// NOLINT(check-name)` comment:

```cpp
int* ptr = nullptr;  // NOLINT(modernize-use-auto) - explicit type clarity
```

### Issue: CI fails but tests pass locally

**Solution**: Ensure you're using the same configuration:

```bash
# Use the same preset as CI
cmake -B build -S . --preset=x64-release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

---

## Pre-Commit Checklist

Before pushing your changes:

- [ ] **Format Code**: `clang-format -style=file -i src/**/*.cpp src/**/*.h`
- [ ] **Run Tests**: `ctest --test-dir build -C Release`
- [ ] **Check for Warnings**: `clang-tidy -p build src/your_file.cpp`
- [ ] **Update Documentation**: If adding new features, update `CLAUDE.md` or `README.md`
- [ ] **Commit Message**: Follow [Conventional Commits](https://www.conventionalcommits.org/) format
  - `feat:` new feature
  - `fix:` bug fix
  - `docs:` documentation changes
  - `refactor:` code refactoring
  - `test:` adding tests
  - `chore:` build/tooling changes

---

## Additional Resources

- **Project Architecture**: See `docs/plans/2025-12-29-modern-aimbot-architecture-design.md`
- **Critical Traps**: See `docs/CRITICAL_TRAPS.md` (read before implementing threading/memory code)
- **Coding Standards**: See `.clang-format` and `.clang-tidy` for full configuration

---

## Getting Help

- **Issues**: [GitHub Issues](https://github.com/MacroMan5/marcoman_ai_aimbot/issues)
- **Discussions**: [GitHub Discussions](https://github.com/MacroMan5/marcoman_ai_aimbot/discussions)

---

**Thank you for contributing to MacroMan AI Aimbot!** 🚀
