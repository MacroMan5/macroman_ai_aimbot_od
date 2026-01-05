# CLAUDE.md

**MacroMan AI Aimbot v2** - C++20 real-time CV pipeline (educational only).

## Skills (Check Before Acting)

Before implementing features or fixing bugs, check if a skill applies:
- **brainstorming** → New features, design decisions
- **writing-plans** → Multi-step tasks needing a plan
- **executing-plans** → Following an existing plan
- **systematic-debugging** → Bugs, test failures, unexpected behavior
- **requesting-code-review** → After completing significant code

## Quick Reference

- **Status:** `docs/plans/State-Action-Sync/STATUS.md`
- **Backlog:** `docs/plans/State-Action-Sync/BACKLOG.md`
- **Critical Traps:** `docs/CRITICAL_TRAPS.md`

## Tech Stack

C++20 (MSVC 2022) | CMake 3.25+ | Catch2 v3 | spdlog | Eigen 3.4 | ONNX Runtime 1.19+ | ImGui 1.91.2 | Windows 10/11

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
```

## Critical Rules

**DO:** `std::unique_ptr`/`std::shared_ptr`, lock-free atomics, pre-allocate in constructors, write tests first
**DON'T:** `std::mutex` in loops, `new`/`delete`, `std::vector::push_back()` in hot paths, read game memory

## Naming

- Classes: PascalCase (`DMLDetector`)
- Functions: camelCase (`captureFrame()`)
- Constants: UPPER_SNAKE_CASE (`MAX_TARGETS`)
- Interfaces: `I` prefix (`IDetector`)

## Performance Budgets

| Stage | Target | P95 |
|-------|--------|-----|
| Capture | <1ms | 2ms |
| Inference | 5-8ms | 10ms |
| Tracking | <1ms | 2ms |
| Input | <0.5ms | 1ms |
| **TOTAL** | **<10ms** | **15ms** |

## Architecture (4-Thread Pipeline)

```
Capture → LatestFrameQueue → Detection → LatestFrameQueue → Tracking → Atomic AimCommand → Input (1000Hz)
```

*Read STATUS.md and BACKLOG.md when needed for current phase details.*
