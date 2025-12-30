# Read-Only Philosophy - Zero Foreign Memory Access

**Pillar 4 of Low-Profile Design**

---

## Table of Contents

1. [The Fundamental Principle](#the-fundamental-principle)
2. [Why Memory Access Is Detectable](#why-memory-access-is-detectable)
3. [Vision-Based vs Memory-Based Approach](#vision-based-vs-memory-based-approach)
4. [Benefits of External Analysis](#benefits-of-external-analysis)
5. [What NOT to Do](#what-not-to-do)
6. [Architectural Enforcement](#architectural-enforcement)
7. [Legal and Ethical Implications](#legal-and-ethical-implications)

---

## The Fundamental Principle

### The Safest Code Is Code That Does Nothing

**Core Philosophy:**

```
┌─────────────────────────────────────────────┐
│  The safest external application is one     │
│  that NEVER touches the game process.       │
│                                             │
│  • No memory reading (PROCESS_VM_READ)      │
│  • No memory writing (PROCESS_VM_WRITE)     │
│  • No DLL injection (CreateRemoteThread)    │
│  • No API hooking (SetWindowsHookEx)        │
│  • No process handles (OpenProcess)         │
└─────────────────────────────────────────────┘
```

**Rationale:**
1. **Legal:** Reading/writing game memory may violate DMCA, CFAA (Computer Fraud and Abuse Act)
2. **Detection:** Memory access is the #1 detection vector for kernel-mode anti-cheats
3. **Ethical:** External vision-based analysis is fundamentally different from memory manipulation

---

## Why Memory Access Is Detectable

### Kernel-Mode Anti-Cheat Detection

**Modern anti-cheats (Vanguard, BattlEye, Easy Anti-Cheat) are kernel drivers that can:**

#### 1. **Monitor OpenProcess Calls**

```cpp
// Kernel-mode driver hooks NtOpenProcess
NTSTATUS HookedNtOpenProcess(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
) {
    // Check if someone is trying to open our game process
    if (ClientId->UniqueProcess == gameProcessId) {
        // Log suspicious access
        if (DesiredAccess & (PROCESS_VM_READ | PROCESS_VM_WRITE)) {
            LogSuspiciousActivity("External process attempting memory access");
            Ban(ClientId->UniqueProcess);
        }
    }

    return RealNtOpenProcess(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}
```

**Result:** ANY attempt to open a handle to the game process is logged.

#### 2. **Enumerate Process Handles**

```cpp
// Anti-cheat periodically scans all open handles
void scanHandles() {
    for (auto& process : allProcesses) {
        auto handles = EnumHandles(process.pid);

        for (auto& handle : handles) {
            if (handle.targetPid == gameProcessId) {
                // Someone has a handle to our game
                if (handle.accessMask & PROCESS_VM_READ) {
                    Ban(process.pid);
                }
            }
        }
    }
}
```

**Result:** Even if you successfully open a handle, the anti-cheat can detect it.

#### 3. **Memory Integrity Checks**

```cpp
// Anti-cheat validates that game memory is unmodified
void validateMemory() {
    uint8_t* playerHealthAddr = (uint8_t*)0x12345678;
    uint32_t expectedHash = HashMemoryRegion(playerHealthAddr, 1024);

    if (GetHash(playerHealthAddr, 1024) != expectedHash) {
        // Memory has been modified
        Ban("Memory tampering detected");
    }
}
```

**Result:** Writing to game memory is instantly detectable.

---

## Vision-Based vs Memory-Based Approach

### Comparison

| Aspect | Memory-Based Aimbot | Vision-Based Aimbot (This Project) |
|--------|---------------------|-------------------------------------|
| **Data Source** | Read player positions from game memory | Analyze screen pixels with computer vision |
| **Detection Risk** | 🔴 Very High (OpenProcess, ReadProcessMemory) | 🟢 Very Low (no game process access) |
| **Kernel Driver Needed** | Often (to bypass anti-cheat) | ❌ Never |
| **DLL Injection** | Usually (to hook game functions) | ❌ Never |
| **Legal Risk** | 🔴 High (violates DMCA, CFAA) | 🟡 Lower (external observation) |
| **Latency** | <1ms (direct memory read) | 5-10ms (capture → AI → input) |
| **Accuracy** | 🔴 Perfect (exact coordinates) | 🟡 Imperfect (vision can miss) |
| **Game Updates** | 🔴 Breaks (memory offsets change) | 🟢 Resilient (visual appearance stable) |
| **Multi-Game Support** | Requires reverse engineering each game | ✅ Same model works across games |

**Fundamental Difference:**

```
Memory-Based:
┌────────────┐       Read Memory      ┌───────────┐
│   Cheat    │ ──────────────────────→ │   Game    │
│  Process   │ ←────────────────────── │  Process  │
└────────────┘     Write Memory        └───────────┘
   ILLEGAL            (Injection)

Vision-Based:
┌────────────┐    Screen Capture      ┌───────────┐
│   Aimbot   │ ───────────────────┐   │   Game    │
│  Process   │                    │   │  Process  │
└────────────┘                    │   └───────────┘
      ↑                           │         │
      │                           └─────────┘
      │                          Display Output
   AI Analysis                   (Public Info)

   LEGAL (External Observation)
```

---

## Benefits of External Analysis

### 1. **No Process Interaction**

**Our architecture:**
```cpp
// ✅ GOOD: Zero interaction with game process
class Engine {
    void run() {
        // Capture screen (Windows API, not game-specific)
        auto frame = screenCapture->captureFrame();

        // AI detection (our own model, no game access)
        auto detections = detector->detect(frame);

        // Move mouse (hardware or Windows API, generic)
        mouseDriver->move(dx, dy);

        // NO OpenProcess()
        // NO ReadProcessMemory()
        // NO WriteProcessMemory()
        // NO EnumProcessModules()
    }
};
```

**Anti-cheat perspective:**
- No suspicious API calls
- No handles to game process
- Indistinguishable from OBS, Discord, or any screen recording software

### 2. **Game Update Resilience**

**Memory-based aimbot:**
```cpp
// ⚠️ Breaks after every game patch
uint64_t playerListOffset = 0x12345678;  // Hardcoded offset
Player* players = *(Player**)(gameBase + playerListOffset);
```

**Vision-based aimbot:**
```cpp
// ✅ Resilient: YOLO model trained on visual appearance
auto detections = detector->detect(frame);  // Works even after patches
```

**Why:** Player models look similar across patches. Memory offsets change constantly.

### 3. **Multi-Game Support**

**Memory-based:** Requires reverse engineering for each game (weeks of work).

**Vision-based:** Train YOLO on multiple games, single codebase.

```cpp
// Same code, different models
if (currentGame == "Valorant") {
    detector->loadModel("models/valorant_yolov8.onnx");
} else if (currentGame == "CS2") {
    detector->loadModel("models/cs2_yolov8.onnx");
}
```

### 4. **Educational Value**

**Vision-based approach teaches:**
- Modern AI (YOLO, object detection)
- Real-time computer vision
- GPU programming (DirectML, TensorRT)
- Control theory (Kalman filters, trajectory planning)

**Memory-based approach teaches:**
- Reverse engineering (less transferable skill)
- Kernel programming (dangerous, illegal in many contexts)
- Hacking techniques (ethical concerns)

---

## What NOT to Do

### Forbidden Operations

#### ❌ **DO NOT: Open Handle to Game Process**

```cpp
// ❌ BAD: Opens handle to game (instant detection)
HANDLE hGame = OpenProcess(PROCESS_VM_READ, FALSE, gamePid);
if (hGame) {
    // Even if you don't read memory, the handle exists
    // Anti-cheat can enumerate handles and find it
}
```

#### ❌ **DO NOT: Read Game Memory**

```cpp
// ❌ BAD: Reads player positions from game memory
DWORD playerPosAddr = 0x12345678;
Vec3 playerPos;
ReadProcessMemory(hGame, (LPCVOID)playerPosAddr, &playerPos, sizeof(playerPos), nullptr);
```

**Why It's Bad:**
- Violates DMCA (circumventing technical protection)
- Detected by kernel-mode hooks
- Illegal in many jurisdictions

#### ❌ **DO NOT: Inject DLL**

```cpp
// ❌ BAD: Injects DLL into game process
LPVOID remoteMem = VirtualAllocEx(hGame, nullptr, 256, MEM_COMMIT, PAGE_READWRITE);
WriteProcessMemory(hGame, remoteMem, dllPath, strlen(dllPath), nullptr);
HANDLE hThread = CreateRemoteThread(hGame, nullptr, 0, LoadLibraryA, remoteMem, 0, nullptr);
```

**Why It's Bad:**
- Kernel drivers monitor CreateRemoteThread
- Writing to game memory triggers integrity checks
- Extremely illegal

#### ❌ **DO NOT: Hook Game APIs**

```cpp
// ❌ BAD: Hooks DirectX Present function
DetourTransactionBegin();
DetourAttach(&(PVOID&)RealPresent, HookedPresent);
DetourTransactionCommit();
```

**Why It's Bad:**
- Requires code injection (DLL or kernel driver)
- Anti-cheat verifies function integrity
- Detectable via IAT/EAT analysis

---

## Architectural Enforcement

### How to Guarantee Read-Only

#### 1. **Code Review Checklist**

Before merging any code:
- [ ] Search for `OpenProcess` → Should be 0 results
- [ ] Search for `ReadProcessMemory` → Should be 0 results
- [ ] Search for `WriteProcessMemory` → Should be 0 results
- [ ] Search for `CreateRemoteThread` → Should be 0 results
- [ ] Search for `VirtualAllocEx` → Should be 0 results

```bash
# Automated check
grep -r "OpenProcess\|ReadProcessMemory\|WriteProcessMemory" src/
# Should return nothing
```

#### 2. **Static Analysis**

Use MSVC's Static Analyzer:
```cmake
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /analyze")
endif()
```

Add custom rule:
```cpp
// In core header
#pragma warning(error: 4996)  // Treat deprecated functions as errors

// Deprecate dangerous functions
#define OpenProcess(...) \
    _Pragma("message(\"OpenProcess is forbidden in read-only architecture\")") \
    OpenProcess_FORBIDDEN(__VA_ARGS__)
```

#### 3. **Interface Contracts**

**IScreenCapture contract:**
```cpp
class IScreenCapture {
public:
    // ✅ Allowed: Capture desktop (Windows API)
    virtual Frame* captureFrame() = 0;

    // ❌ Forbidden: Access game process
    // (not even declared)
    // virtual HANDLE openGameProcess(DWORD pid) = 0;
};
```

**If an interface doesn't declare a method, it can't be called.**

#### 4. **Build-Time Verification**

Add a static assert:
```cpp
// In main.cpp
static_assert(
    std::is_same_v<decltype(&OpenProcess), void*>,
    "OpenProcess must not be linked (read-only violation)"
);

// This will fail to compile if OpenProcess is used anywhere
```

---

## Legal and Ethical Implications

### Why Read-Only Matters Legally

**DMCA (Digital Millennium Copyright Act):**
- Section 1201: Prohibits circumvention of technical protection measures
- Reading encrypted game memory = circumvention
- **Vision-based analysis**: Not circumvention (analyzing public display output)

**CFAA (Computer Fraud and Abuse Act):**
- Accessing a computer without authorization
- Reading game memory without developer permission = unauthorized access
- **External screen capture**: Authorized (using Windows APIs as intended)

**Terms of Service:**
- Most games prohibit "third-party software that provides an unfair advantage"
- Memory reading clearly violates this
- **Vision-based tools**: Gray area (depends on interpretation)

### Ethical Considerations

**Memory-Based:**
- Directly manipulates game state
- Gives perfect information (wallhacks, ESP)
- Fundamentally unfair

**Vision-Based (This Project):**
- Only uses information visible to human player
- Imperfect (can miss targets, fooled by visual tricks)
- **Still cheating in competitive games, but architecturally different**

**Acceptable Use Cases:**
1. **Educational research** - Learning AI/CV
2. **Single-player games** - No harm to others
3. **Authorized testing** - With developer permission
4. **Accessibility** - Assisting disabled gamers (with permission)

---

## Comparison to Legitimate Software

### What We Look Like to Anti-Cheat

**Our process:**
```
macroman_aimbot.exe
├── Screen Capture (WinRT API)
├── AI Inference (ONNX Runtime)
├── Mouse Input (Arduino USB HID)
└── Overlay (ImGui window)
```

**Similar to:**
- **OBS Studio** (screen recording software)
- **Discord** (screen sharing)
- **NVIDIA ShadowPlay** (instant replay)
- **AutoHotkey** (automation, but we use hardware input)

**Anti-cheat cannot distinguish us from OBS** because we use the same APIs.

**What sets us apart:**
- We use AI to analyze the captured frames
- We send mouse input based on that analysis

**But:** We never touch the game process, so from anti-cheat's perspective, we're just "screen recording software that controls a USB mouse."

---

## Conclusion

**The Read-Only Philosophy is our strongest defense.**

**Key Principles:**
1. **Never open a handle to the game process** (`OpenProcess`)
2. **Never read game memory** (`ReadProcessMemory`)
3. **Never write to game memory** (`WriteProcessMemory`)
4. **Never inject code** (`CreateRemoteThread`)
5. **Only use public Windows APIs** (screen capture, input)

**Benefits:**
- **Legal:** External observation vs internal manipulation
- **Detection:** Anti-cheat cannot distinguish from OBS/Discord
- **Maintainability:** No reverse engineering required
- **Resilience:** Survives game patches

**Verification Checklist:**
- [ ] `grep -r "OpenProcess" src/` → 0 results
- [ ] `grep -r "ReadProcessMemory" src/` → 0 results
- [ ] `grep -r "WriteProcessMemory" src/` → 0 results
- [ ] `dumpbin /imports macroman_aimbot.exe` → No suspicious APIs
- [ ] Code review: All interfaces use only Windows screen/input APIs

**Remember:**
```
If you don't access the game process,
the anti-cheat has nothing to detect.
```

**Related Documents:**
- [Hardware Input Emulation](./01-hardware-input-emulation.md) - Why we use Arduino instead of WriteProcessMemory
- [Main Architecture](../plans/2025-12-29-modern-aimbot-architecture-design.md) - Section 3: External process design

---

**Last Updated:** 2025-12-29
