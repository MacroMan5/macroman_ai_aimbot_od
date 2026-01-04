# MacroMan AI Aimbot v2 - User Setup & Calibration Guide

**Target Audience:** Educational testers and researchers
**Prerequisites:** Windows 10/11, NVIDIA or AMD GPU with DirectX 12 support
**Last Updated:** 2026-01-04

---

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Installation](#installation)
3. [First-Time Setup](#first-time-setup)
4. [Configuration Overview](#configuration-overview)
5. [Calibration Guide](#calibration-guide)
6. [Performance Tuning](#performance-tuning)
7. [Troubleshooting](#troubleshooting)

---

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 10 (version 1903+) or Windows 11 |
| **CPU** | Intel i5-8400 / AMD Ryzen 5 2600 (6 cores) |
| **RAM** | 8 GB |
| **GPU** | NVIDIA GTX 1060 6GB / AMD RX 580 8GB |
| **DirectX** | DirectX 12 capable GPU with WDDM 2.4+ driver |
| **Storage** | 2 GB free space |

### Recommended Requirements

| Component | Requirement |
|-----------|-------------|
| **OS** | Windows 11 (latest updates) |
| **CPU** | Intel i7-10700K / AMD Ryzen 7 5800X (8+ cores) |
| **RAM** | 16 GB |
| **GPU** | NVIDIA RTX 3060 / AMD RX 6700 XT |
| **Storage** | SSD with 5 GB free space |

### GPU Driver Requirements

**CRITICAL:** Outdated GPU drivers cause 50-100% performance degradation.

- **NVIDIA:** GeForce Game Ready Driver 531.0 or newer
  - Download: https://www.nvidia.com/Download/index.aspx
  - Verify: `nvidia-smi` in Command Prompt should show driver version

- **AMD:** Adrenalin 23.2.1 or newer
  - Download: https://www.amd.com/en/support
  - Enable DirectML acceleration in Radeon Software settings

**Verification:**
```bash
# Check DirectX version
dxdiag

# Should show:
# - DirectX Version: DirectX 12
# - Feature Levels: 12_1 or higher
```

---

## Installation

### Step 1: Download Release

**Option A: Pre-built Binary (Recommended)**
1. Go to [Releases](https://github.com/MacroMan5/marcoman_ai_aimbot/releases)
2. Download `MacroMan-Aimbot-v2.x.x-Windows.zip`
3. Extract to `C:\MacroMan\` (avoid paths with spaces)

**Option B: Build from Source**
```bash
git clone https://github.com/MacroMan5/marcoman_ai_aimbot.git
cd marcoman_ai_aimbot

# Configure and build (Release mode)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j

# Binaries in: build/bin/Release/
```

### Step 2: Download YOLO Model

**Required:** YOLO v8 Nano ONNX model (FPS player detection)

1. Download `sunxds_0.7.3.onnx` from [ModelZoo](https://github.com/SunXDS/yolov8-fps-models/releases)
2. Place in `assets/models/sunxds_0.7.3.onnx`

**File structure:**
```
C:\MacroMan\
├── macroman_aimbot.exe
├── assets\
│   └── models\
│       └── sunxds_0.7.3.onnx   <- Place here
└── config\
    ├── global.ini
    └── profiles\
        ├── valorant.json
        └── csgo.json
```

### Step 3: Verify Installation

```bash
cd C:\MacroMan
.\macroman_aimbot.exe --version

# Expected output:
# MacroMan AI Aimbot v2.0.0
# DirectML Backend: Ready
# ONNX Runtime: 1.19.2
```

---

## First-Time Setup

### 1. Configure Global Settings

Edit `config/global.ini`:

```ini
[Display]
TargetFPS = 144          # Capture frame rate (60/144/240)
ScreenWidth = 1920       # Your monitor resolution
ScreenHeight = 1080

[Detection]
ModelPath = assets/models/sunxds_0.7.3.onnx
ConfidenceThreshold = 0.5    # Min detection confidence (0.3-0.7)
NMSThreshold = 0.4           # Non-maximum suppression (0.3-0.5)

[Tracking]
GracePeriod = 500            # Lost target timeout (ms)
MaxExtrapolation = 50        # Max prediction time (ms)

[Input]
MouseSensitivity = 1.0       # Game sensitivity multiplier
InputFrequency = 1000        # Hz (do not change)

[Humanization]
EnableReactionDelay = true
MinReactionDelay = 50        # Min delay before aiming (ms)
MaxReactionDelay = 150       # Max delay (ms)

EnableTremor = true
TremorAmplitude = 1.5        # Pixel jitter (0-3)

EnableBezierOvershoot = true
OvershootFactor = 0.15       # Overshoot amount (0-0.3)
```

### 2. Select Game Profile

Profiles customize detection for specific games.

**Available Profiles:**
- `valorant.json` - Valorant (optimized for character models)
- `csgo.json` - Counter-Strike: Global Offensive
- `apex.json` - Apex Legends (WIP)

**Auto-Detection:**
The system automatically loads the correct profile when you launch a supported game.

**Manual Override:**
```bash
.\macroman_aimbot.exe --profile config/profiles/valorant.json
```

### 3. First Run

1. **Launch Game First** (windowed or borderless mode)
2. **Run Aimbot:**
   ```bash
   .\macroman_aimbot.exe
   ```
3. **Verify Output:**
   ```
   [INFO] Initializing MacroMan Aimbot v2.0.0
   [INFO] Loading model: assets/models/sunxds_0.7.3.onnx
   [INFO] DirectML backend initialized (GPU: NVIDIA RTX 3060)
   [INFO] Profile detected: Valorant
   [INFO] Capture started: 1920x1080 @ 144 FPS
   [INFO] Pipeline ready. Latency: 9.2ms (Target: <10ms)
   ```

4. **Test Detection:**
   - Enter game training mode
   - Point at player model
   - Debug overlay should show green bounding box
   - Press `F1` to toggle overlay visibility

---

## Configuration Overview

### Global Config (`config/global.ini`)

**Purpose:** System-wide settings that apply to all games.

**Key Settings:**

| Section | Parameter | Description | Range |
|---------|-----------|-------------|-------|
| Display | TargetFPS | Capture frame rate | 60, 144, 240 |
| Detection | ConfidenceThreshold | Min detection score | 0.3 - 0.7 |
| Detection | NMSThreshold | Overlap filtering | 0.3 - 0.5 |
| Tracking | GracePeriod | Lost target timeout | 200 - 1000 ms |
| Input | MouseSensitivity | Game sens multiplier | 0.5 - 2.0 |
| Humanization | MinReactionDelay | Reaction delay min | 30 - 100 ms |
| Humanization | TremorAmplitude | Hand shake simulation | 0.5 - 3.0 px |

### Game Profiles (`config/profiles/*.json`)

**Purpose:** Game-specific detection tuning.

**Example:** `valorant.json`
```json
{
  "gameName": "Valorant",
  "processName": "VALORANT-Win64-Shipping.exe",
  "windowTitle": "VALORANT",

  "fov": 60.0,
  "aimSmoothness": 0.5,
  "targetPriority": "closest",

  "hitboxPriority": ["head", "chest", "body"],

  "humanization": {
    "reactionDelay": {
      "min": 50,
      "max": 150
    },
    "tremor": {
      "amplitude": 1.5,
      "frequency": 8.0
    }
  }
}
```

**Customization:**
1. Copy existing profile: `cp valorant.json my_game.json`
2. Edit `processName` and `windowTitle` to match your game
3. Tune FOV and smoothness (see Calibration section)

---

## Calibration Guide

### Step 1: FOV (Field of View) Tuning

**Purpose:** Define the circular region around crosshair for target selection.

**Visual Reference:**
```
      Screen (1920x1080)
  ┌─────────────────────────┐
  │                         │
  │         ┌───────┐       │
  │         │  FOV  │       │  <- FOV = 60° (radius ~200px)
  │         │   ●   │       │     Center = crosshair
  │         └───────┘       │
  │                         │
  └─────────────────────────┘
```

**Tuning Process:**

1. **Start with default:** `"fov": 60.0`
2. **Test in-game:**
   - Stand at medium range (~10m) from target
   - Move crosshair near (but not on) target
   - Does aimbot activate? → FOV is appropriate
   - Aimbot activates when looking away? → FOV too large (reduce to 45)
   - Aimbot doesn't activate when close to target? → FOV too small (increase to 80)

3. **Adjust based on playstyle:**
   - **Aggressive (close quarters):** FOV 45-60° (smaller area, faster snap)
   - **Defensive (long range):** FOV 70-90° (wider area, smoother tracking)

**Formula:**
```
FOV (degrees) ≈ 2 × arctan(radiusPixels / screenWidth) × (180/π)
```

**Common Values:**
- 1920x1080: 60° ≈ 200px radius
- 2560x1440: 60° ≈ 267px radius
- 3840x2160: 60° ≈ 400px radius

---

### Step 2: Aim Smoothness Tuning

**Purpose:** Control how quickly the aimbot moves the mouse toward the target.

**Range:** `0.0` (instant snap) to `1.0` (very smooth)

**Effect:**
```
aimSmoothness = 0.0:  ●━━━━━━━━━━━━━━━━━━━━►◆  (Instant, robotic)
aimSmoothness = 0.5:  ●─────────────────────►◆  (Balanced)
aimSmoothness = 1.0:  ●～～～～～～～～～～～～►◆  (Slow, human-like)
```

**Tuning Process:**

1. **Start with:** `"aimSmoothness": 0.5`
2. **Test tracking:**
   - Follow moving target for 5 seconds
   - Does crosshair "snap" instantly? → Too low (increase to 0.6-0.7)
   - Does crosshair lag behind target? → Too high (decrease to 0.3-0.4)

3. **Adjust based on detection confidence:**
   - **High confidence (close range):** Lower smoothness (0.3-0.5) for faster reaction
   - **Low confidence (far range):** Higher smoothness (0.6-0.8) to avoid jitter

**Anti-Cheat Considerations:**
- `aimSmoothness < 0.3`: Highly detectable (instant snaps)
- `aimSmoothness 0.5-0.7`: Recommended (human-like)
- `aimSmoothness > 0.9`: Too slow for competitive play

---

### Step 3: 1 Euro Filter Tuning (Advanced)

**Purpose:** Smooth noisy detection predictions while maintaining low latency.

**Parameters:**
```json
"oneEuroFilter": {
  "minCutoff": 1.0,     // Lower = more smoothing
  "beta": 0.007,        // Higher = more responsive to velocity
  "dCutoff": 1.0        // Derivative cutoff
}
```

**When to Adjust:**
- **Jittery crosshair:** Decrease `minCutoff` to 0.5
- **Lagging behind target:** Increase `beta` to 0.01
- **Overshooting:** Decrease `beta` to 0.005

**Default values work for 90% of use cases.** Only adjust if you observe specific issues.

---

### Step 4: Hitbox Priority

**Purpose:** Define which body part to aim for.

**Options:**
```json
"hitboxPriority": ["head", "chest", "body"]
```

**Behavior:**
1. Aimbot searches for "head" hitbox first
2. If no head detected, falls back to "chest"
3. If no chest, aims for "body" (center mass)

**Tuning:**
- **High skill games (CS:GO, Valorant):** `["head", "chest"]` (headshot priority)
- **Fast-paced games (Apex, Overwatch):** `["chest", "head"]` (easier tracking)
- **Training mode:** `["body"]` (always aims center mass)

---

## Performance Tuning

### Monitoring Performance

**In-Game Overlay (F1 to toggle):**
```
┌─────────────────────────────┐
│ FPS: 144.2                  │
│ Latency:                    │
│   Capture:    0.8ms         │
│   Detection:  9.2ms   ⚠     │  <- Bottleneck detected
│   Tracking:   0.4ms         │
│   Input:      0.3ms         │
│                             │
│ Active Targets: 2           │
│ VRAM Usage: 256 MB / 512 MB │
└─────────────────────────────┘
```

**Bottleneck Detection:**
- ⚠ **Detection > 15ms:** Reduce model resolution or switch to TensorRT backend
- ⚠ **Capture > 2ms:** Update GPU drivers or lower TargetFPS
- ⚠ **Tracking > 2ms:** Too many targets (increase confidence threshold)

---

### Optimization Tips

#### 1. Reduce Detection Latency (8-12ms → 5-8ms)

**Option A: Lower Model Resolution**

Edit `config/global.ini`:
```ini
[Detection]
DetectionResolution = 0.75   # 640x640 → 480x480 (25% faster)
```

**Trade-off:** Lower accuracy at long range.

---

**Option B: Switch to TensorRT Backend (NVIDIA only)**

1. Install CUDA Toolkit 12.0: https://developer.nvidia.com/cuda-downloads
2. Install TensorRT 8.6: https://developer.nvidia.com/tensorrt
3. Rebuild with TensorRT:
   ```bash
   cmake -B build -S . -DENABLE_TENSORRT=ON
   cmake --build build --config Release
   ```

**Performance:** 5-8ms inference (vs 8-12ms DirectML)

---

#### 2. Reduce Capture Latency (0.8ms → 0.5ms)

**Update GPU Drivers:**
```bash
# NVIDIA: Check version
nvidia-smi

# AMD: Open Radeon Software → Settings → System
```

**Windows Graphics Settings:**
1. Open Settings → System → Display → Graphics settings
2. Add `macroman_aimbot.exe`
3. Set to "High Performance"

---

#### 3. CPU Thread Affinity (Advanced)

**Automatic Assignment:**
The system automatically assigns threads to CPU cores:
- Capture → Core 0
- Detection → Core 1
- Tracking → Core 2
- Input → Core 3

**Manual Override:** Edit `config/global.ini`
```ini
[Threading]
CaptureCore = 0
DetectionCore = 1
TrackingCore = 2
InputCore = 3
```

**Recommendation:** Only change if you have >8 cores and want to isolate from game process.

---

## Troubleshooting

### Issue: "Failed to initialize DirectML"

**Symptoms:**
```
[ERROR] Failed to initialize DirectML backend
[ERROR] GPU not compatible or driver outdated
```

**Solutions:**
1. **Update GPU drivers** (see GPU Driver Requirements)
2. **Check DirectX 12 support:**
   ```bash
   dxdiag
   # Feature Levels should show 12_0 or higher
   ```
3. **Reinstall DirectX Runtime:**
   - Download: https://www.microsoft.com/en-us/download/details.aspx?id=35
4. **Fallback to CPU mode** (slow, not recommended):
   ```bash
   .\macroman_aimbot.exe --cpu-only
   ```

---

### Issue: "Model not found"

**Symptoms:**
```
[ERROR] Failed to load model: assets/models/sunxds_0.7.3.onnx
[ERROR] File not found
```

**Solutions:**
1. Verify file exists:
   ```bash
   dir assets\models\sunxds_0.7.3.onnx
   ```
2. Download from [ModelZoo](https://github.com/SunXDS/yolov8-fps-models/releases)
3. Check file size: Should be ~12 MB
4. Ensure no antivirus quarantine (whitelist `assets/` folder)

---

### Issue: High detection latency (>20ms)

**Symptoms:**
- Overlay shows "Detection: 25ms" (red warning)
- Choppy aiming, delayed reactions

**Solutions:**
1. **Lower detection resolution:**
   ```ini
   DetectionResolution = 0.5   # 640x640 → 320x320
   ```
2. **Reduce TargetFPS:**
   ```ini
   TargetFPS = 60   # 144 → 60 FPS
   ```
3. **Close background applications** (Chrome, Discord, streaming software)
4. **Check GPU temperature:** >85°C causes thermal throttling
   ```bash
   # NVIDIA:
   nvidia-smi --query-gpu=temperature.gpu --format=csv
   ```

---

### Issue: Aimbot doesn't activate

**Symptoms:**
- Debug overlay shows "Active Targets: 0" even when aiming at enemy
- No bounding boxes visible

**Solutions:**
1. **Lower confidence threshold:**
   ```ini
   ConfidenceThreshold = 0.3   # Default: 0.5
   ```
2. **Increase FOV:**
   ```json
   "fov": 80.0   // Wider search area
   ```
3. **Verify game window is active:**
   - Aimbot only works when game window has focus
   - Check overlay title: Should match game name
4. **Check model compatibility:**
   - `sunxds_0.7.3.onnx` is trained on Valorant/CS:GO character models
   - Other games may need custom-trained models

---

### Issue: Mouse movement feels "robotic"

**Symptoms:**
- Instant snaps to target
- No overshoot or hesitation
- Clearly non-human behavior

**Solutions:**
1. **Increase aimSmoothness:**
   ```json
   "aimSmoothness": 0.7   // Default: 0.5
   ```
2. **Enable all humanization features:**
   ```ini
   EnableReactionDelay = true
   EnableTremor = true
   EnableBezierOvershoot = true
   ```
3. **Increase reaction delay:**
   ```json
   "reactionDelay": {
     "min": 80,    // Slower reactions
     "max": 200
   }
   ```
4. **Tune tremor amplitude:**
   ```json
   "tremor": {
     "amplitude": 2.5   // More hand shake
   }
   ```

---

## Next Steps

After completing setup and calibration:

1. **Read Safety Guide:** `docs/SAFETY_ETHICS.md`
   - Understand prohibited use cases
   - Ethical considerations
   - Legal disclaimers

2. **Review FAQ:** `docs/FAQ.md`
   - Common implementation traps
   - Component-specific issues
   - Performance optimization

3. **Join Community:**
   - Discord: https://discord.gg/macroman
   - Reddit: r/MacroManDev
   - GitHub Discussions: https://github.com/MacroMan5/marcoman_ai_aimbot/discussions

---

## Legal Disclaimer

⚠ **EDUCATIONAL USE ONLY**

MacroMan AI Aimbot v2 is provided for **educational and research purposes only**. Usage in online multiplayer games violates Terms of Service and may result in:
- Permanent account bans
- Hardware ID bans
- Legal action from game publishers

**By using this software, you acknowledge:**
- You will NOT use it in online competitive games
- You understand anti-cheat detection risks
- You accept full responsibility for consequences

**Recommended Use Cases:**
- ✅ Offline training modes
- ✅ Private servers with consent
- ✅ Computer vision research
- ✅ Game AI development
- ❌ Online matchmaking (ANY GAME)
- ❌ Tournaments or competitive play
- ❌ Streaming with aimbot active

For full ethical guidelines, see `docs/SAFETY_ETHICS.md`.
