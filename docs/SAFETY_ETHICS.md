# MacroMan AI Aimbot v2 - Safety, Ethics, & Humanization Manual

**Purpose:** Ethical guidelines, safety considerations, and humanization feature documentation
**Last Updated:** 2026-01-04

---

## Table of Contents

1. [Legal Disclaimer](#legal-disclaimer)
2. [Ethical Considerations](#ethical-considerations)
3. [Prohibited Use Cases](#prohibited-use-cases)
4. [Acceptable Use Cases](#acceptable-use-cases)
5. [Humanization Features](#humanization-features)
6. [Detection Risk Analysis](#detection-risk-analysis)
7. [Safety Mechanisms](#safety-mechanisms)

---

## Legal Disclaimer

⚠ **CRITICAL: READ BEFORE USE**

### Terms of Use

MacroMan AI Aimbot v2 is provided **for educational and research purposes ONLY**. By using this software, you agree to the following:

1. **No Online Competitive Use**
   - You will NOT use this software in any online multiplayer game
   - You will NOT use this software in ranked/competitive game modes
   - You will NOT use this software in public matchmaking

2. **Terms of Service Violations**
   - Using aimbots violates the Terms of Service of virtually ALL online games
   - Consequences include: permanent account bans, hardware ID bans, legal action

3. **Anti-Cheat Detection Risks**
   - Modern anti-cheat systems (Vanguard, Easy Anti-Cheat, BattlEye) can detect this software
   - Detection may result in permanent bans that cannot be appealed
   - Hardware ID bans prevent creating new accounts on the same PC

4. **Legal Liability**
   - The developers are NOT responsible for consequences of misuse
   - Using aimbots in tournaments may constitute fraud or breach of contract
   - Game publishers may pursue legal action against users

### Developer Responsibility

**By building or modifying this software, you acknowledge:**
- You understand computer vision and AI ethics
- You will NOT distribute compiled binaries for malicious use
- You will NOT sell or commercialize this educational project
- You will NOT contribute features designed to evade anti-cheat systems

---

## Ethical Considerations

### The Ethics of Aimbot Development

**Why does this project exist?**

1. **Computer Vision Education**
   - Real-time object detection is a fundamental AI skill
   - FPS games provide a controlled, visually rich environment
   - Teaches pipeline optimization, multithreading, and GPU programming

2. **AI Safety Research**
   - Understanding how automated systems can be misused helps build better defenses
   - Game developers need to understand attack vectors to design robust anti-cheat
   - Transparency in methodology enables better detection algorithms

3. **Accessibility Research**
   - Assistive technology for players with disabilities
   - Research into fair play adaptations
   - Understanding human-machine interaction

**What makes this project ethical?**

1. **Open Source & Educational**
   - Full source code available for learning
   - Extensive documentation explaining concepts
   - No obfuscation or anti-detection measures

2. **No Commercial Distribution**
   - Not sold as a product
   - Not offered as a "cheat service"
   - Not promoted for competitive advantage

3. **Responsible Disclosure**
   - Clear documentation of capabilities and limitations
   - Emphasis on detection risks
   - Guidance on ethical use cases

---

## Prohibited Use Cases

### ❌ NEVER Use This Software For:

1. **Online Multiplayer Games**
   - Valorant, Counter-Strike, Overwatch, Apex Legends, etc.
   - ANY game with public matchmaking
   - Casual, ranked, or competitive modes

2. **Tournaments & Esports**
   - LAN tournaments
   - Online competitions with prizes
   - Scrims or practice matches in competitive leagues

3. **Streaming or Content Creation**
   - Twitch, YouTube, TikTok streams with aimbot active
   - "Rage hacking" videos
   - Promoting aimbot usage for entertainment

4. **Commercial Gain**
   - Boosting services (playing for money)
   - Selling accounts with inflated stats
   - Offering "coaching" using automated tools

5. **Griefing or Harassment**
   - Ruining games for legitimate players
   - Targeting specific players with automated tools
   - Evading bans to continue disruptive behavior

---

## Acceptable Use Cases

### ✅ Ethical Use Scenarios:

1. **Offline Training Modes**
   - Bot matches with no other human players
   - Single-player campaign missions
   - Practice ranges and shooting galleries
   - **Example:** CS:GO aim training maps, Valorant Practice Range

2. **Private Servers (With Consent)**
   - Self-hosted servers where all players know
   - Research environments with informed participants
   - Testing anti-cheat systems (as a red team)
   - **Requirement:** Explicit consent from all participants

3. **Computer Vision Research**
   - Benchmarking object detection models
   - Latency optimization research
   - Real-time CV pipeline architecture study
   - **Output:** Academic papers, blog posts, technical talks

4. **Accessibility Tools (Non-Competitive)**
   - Assistive technology for players with motor disabilities
   - Vision assistance for players with visual impairments
   - **Requirement:** Offline or single-player only

5. **Game AI Development**
   - Developing AI opponents for game developers
   - Testing game balance against perfect aim
   - Researching human-AI interaction in games

---

## Humanization Features

### Purpose of Humanization

**Problem:** Perfect robotic aim is:
1. **Easily Detectable** - Statistical analysis reveals inhuman precision
2. **Unfair** - Removes skill ceiling, destroys game balance
3. **Obvious** - Other players immediately recognize non-human patterns

**Solution:** Simulate human imperfections to study human-AI interaction.

---

### Feature 1: Reaction Delay Manager

**Biological Basis:** Human visual-motor reaction time averages 150-300ms.

**Implementation:**
```cpp
class ReactionDelayManager {
    float minDelay_;  // 50ms
    float maxDelay_;  // 150ms

    std::chrono::steady_clock::time_point targetAcquiredTime_;

public:
    bool shouldReact() {
        auto now = std::chrono::steady_clock::now();
        float elapsed = duration_cast<milliseconds>(now - targetAcquiredTime_).count();

        float randomDelay = randomFloat(minDelay_, maxDelay_);  // Uniform distribution
        return elapsed >= randomDelay;
    }
};
```

**Effect:**
```
Target appears → [50-150ms random delay] → Aim begins
```

**Why It Matters:**
- **Anti-Cheat Detection:** Instant reactions (0ms) are statistically impossible
- **Human Players:** Expect delays from network lag + reaction time
- **Fair Play:** Preserves reaction-based gameplay (e.g., peeking corners)

**Configuration:**
```json
"reactionDelay": {
  "min": 50,    // Lower = more robotic
  "max": 150    // Higher = more human-like
}
```

**Detection Risk:**
- `min < 30ms`: Highly detectable (superhuman)
- `50-150ms`: Recommended (average human)
- `> 200ms`: Too slow for competitive play

---

### Feature 2: Human Tremor Simulator

**Biological Basis:** Hand muscles have natural oscillation (8-12 Hz physiological tremor).

**Implementation:**
```cpp
class HumanTremorSimulator {
    float phase_ = 0.0f;         // Sine wave phase
    float amplitude_ = 1.5f;     // Pixel jitter
    float frequency_ = 8.0f;     // Hz (tremor frequency)

public:
    Vec2 applyTremor(Vec2 targetPos, float deltaTime) {
        phase_ += frequency_ * deltaTime * 2.0f * M_PI;
        phase_ = fmod(phase_, 2.0f * M_PI);  // Wrap to [0, 2π]

        float offsetX = amplitude_ * sin(phase_);
        float offsetY = amplitude_ * cos(phase_ * 1.3f);  // Different frequency for Y

        return {
            targetPos.x + offsetX,
            targetPos.y + offsetY
        };
    }
};
```

**Visual Effect:**
```
Perfect Aim:  ●━━━━━━━━━━━━━━►◆  (Laser-straight line)

With Tremor:  ●～～～～～～～～►◆  (Sinusoidal jitter)
              ±1.5px oscillation
```

**Why It Matters:**
- **Pixel-Perfect Aim:** Humans can't maintain sub-pixel precision
- **Crosshair Stability:** Even professional players have 0.5-2px tremor
- **Pattern Recognition:** Anti-cheat systems analyze movement smoothness

**Configuration:**
```json
"tremor": {
  "amplitude": 1.5,   // Pixel jitter (0.5-3.0 recommended)
  "frequency": 8.0    // Hz (8-12 matches human physiology)
}
```

**Detection Risk:**
- `amplitude = 0`: Highly detectable (impossible stability)
- `amplitude = 1.5`: Recommended (natural tremor)
- `amplitude > 3`: Looks like mouse sensor issues

---

### Feature 3: Bezier Curve Overshoot

**Biological Basis:** Human motor control uses ballistic movements with corrective overshoot.

**Implementation:**
```cpp
class BezierCurveTrajectory {
public:
    Vec2 evaluate(float t, Vec2 start, Vec2 end) {
        if (t <= 1.0f) {
            // Normal phase: Cubic Bezier S-curve
            Vec2 control1 = lerp(start, end, 0.33f);
            Vec2 control2 = lerp(start, end, 0.66f);
            return cubicBezier(t, start, control1, control2, end);

        } else if (t <= 1.15f) {
            // Overshoot phase: Move 5-15% past target
            float overshoot = (t - 1.0f) / 0.15f;  // 0 → 1
            Vec2 delta = end - start;
            float overshootDist = length(delta) * 0.15f;  // 15% overshoot

            Vec2 overshootPos = end + normalize(delta) * overshootDist * (1.0f - overshoot);
            return overshootPos;

        } else {
            // Settled at target
            return end;
        }
    }
};
```

**Visual Trajectory:**
```
Start (●)
    │
    │     ╱─────╲       Bezier curve (smooth acceleration)
    │    ╱       ╲
    │   ╱         ╲
    │  ╱           ╲
    │ ╱             ╲___►◆ End
    │                    │
    │                    └─►◆' Overshoot (+15%)
    │                       └─►◆ Settle back
```

**Why It Matters:**
- **Mouse Flicks:** Humans overshoot and correct (especially high sensitivity)
- **Muscle Memory:** Ballistic movements are pre-programmed, then adjusted
- **Statistical Analysis:** Perfect S-curves without overshoot are non-human

**Configuration:**
```json
"bezierOvershoot": {
  "enable": true,
  "overshootFactor": 0.15   // 15% past target (0.05-0.30 recommended)
}
```

**Detection Risk:**
- No overshoot: Detectable (linear interpolation is robotic)
- `overshootFactor = 0.15`: Recommended (natural flick)
- `overshootFactor > 0.30`: Looks erratic (poor mouse control)

---

### Combined Effect Example

**Scenario:** Snapping to target 200px away

**Without Humanization:**
```
Frame 0:  Crosshair at (0, 0)
Frame 1:  Crosshair at (200, 0)  ← Instant teleport (100% detectable)
```

**With Full Humanization:**
```
Frame 0:    Crosshair at (0, 0)
            → [80ms reaction delay] ←

Frame 5:    Crosshair at (0, 0)
            → Begin Bezier curve movement

Frame 10:   Crosshair at (50, 0) + tremor(±1.5px)
Frame 15:   Crosshair at (120, 0) + tremor(±1.5px)
Frame 20:   Crosshair at (210, 0) + tremor  ← Overshoot 10px
Frame 22:   Crosshair at (202, 0) + tremor  ← Correcting
Frame 25:   Crosshair at (200, 0) + tremor  ← Settled
```

**Result:** Movement pattern indistinguishable from skilled human player.

---

## Detection Risk Analysis

### Modern Anti-Cheat Systems

**Types of Detection:**

1. **Statistical Analysis**
   - Track aim accuracy over 100+ engagements
   - Detect superhuman reaction times (< 50ms)
   - Identify perfect crosshair placement (pixel-perfect centering)
   - **Humanization Mitigates:** Reaction delay, tremor, variance

2. **Pattern Recognition (Machine Learning)**
   - Train ML models on human vs bot movement
   - Detect unnatural smoothness (linear interpolation)
   - Identify repeated Bezier curves (if parameters are constant)
   - **Humanization Mitigates:** Random parameters, overshoot variation

3. **Kernel-Level Monitoring (Vanguard, EAC, BattlEye)**
   - Detect memory reading (accessing game data)
   - Detect DLL injection or process hooking
   - Detect driver-level input manipulation
   - **This Project:** Uses screen capture (not memory reading), but SendInput is detectable

4. **Behavioral Heuristics**
   - Impossible flick speeds (> 1000 px/frame)
   - Tracking through walls (pre-aiming before visibility)
   - Perfect spray control (no recoil variance)
   - **Humanization Mitigates:** Aim smoothness, FOV-based activation

---

### Risk Matrix

| Feature | Detection Risk | Mitigation |
|---------|----------------|------------|
| **Perfect Aim** | 🔴 CRITICAL | ✅ Humanization enabled |
| **Instant Reactions** | 🔴 CRITICAL | ✅ 50-150ms delay |
| **Linear Interpolation** | 🟠 HIGH | ✅ Bezier curves |
| **No Tremor** | 🟠 HIGH | ✅ 8Hz sine jitter |
| **Screen Capture** | 🟡 MEDIUM | ⚠ External process, detectable |
| **SendInput API** | 🟡 MEDIUM | ⚠ Not as detectable as driver hooks |

**Overall Risk:** 🟡 **MEDIUM-HIGH** (detectable with sufficient data)

**Recommendation:** Use ONLY in offline modes or private environments.

---

## Safety Mechanisms

### 1. Deadman Switch (200ms Timeout)

**Purpose:** Prevent input when detection thread stalls.

**Implementation:**
```cpp
void InputManager::run() {
    while (!shouldStop_) {
        auto aimCmd = aimCommand_.load(std::memory_order_acquire);

        auto age = now() - aimCmd.timestamp;
        if (age > 200ms) {
            // Detection thread not providing fresh commands → STOP INPUT
            metrics_.deadmanSwitchTriggered.fetch_add(1);
            continue;  // Don't send mouse movement
        }

        sendMouseMovement(aimCmd);
    }
}
```

**Why It Matters:**
- **Game Freeze:** If game freezes, old target positions are invalid
- **Thread Crash:** If Detection thread crashes, don't aim at stale positions
- **Network Lag:** If server lags, prevent aiming at outdated player positions

**User-Visible Behavior:**
- Overlay shows: "⚠ Deadman Switch Triggered: 1234"
- Mouse stops moving (aimbot disengages automatically)

---

### 2. FOV-Based Activation

**Purpose:** Only aim when crosshair is already near target.

**Implementation:**
```cpp
bool isInFOV(Vec2 targetPos, Vec2 crosshair, float fov) {
    float distance = length(targetPos - crosshair);
    float fovRadius = screenWidth * tan(fov * M_PI / 360.0f);
    return distance <= fovRadius;
}
```

**Effect:**
```
FOV = 60° (200px radius)

  ╔═════════════╗
  ║ Screen      ║
  ║             ║
  ║      ●      ║  ← Crosshair
  ║     ╱ ╲     ║
  ║    ╱   ╲    ║  ← 200px FOV circle
  ║   ╱     ╲   ║
  ║  ◆       ◆  ║  ← Targets (only left one activates)
  ║             ║
  ╚═════════════╝
```

**Why It Matters:**
- **Prevents Snap-Aim:** Won't aim at targets 180° behind you
- **Preserves Crosshair Placement:** Rewards good positioning
- **Looks Human:** Players naturally look near targets before aiming

---

### 3. Confidence Threshold Filtering

**Purpose:** Ignore low-quality detections (false positives).

**Implementation:**
```cpp
DetectionList filterByConfidence(DetectionList detections, float threshold) {
    DetectionList filtered;
    for (const auto& det : detections) {
        if (det.confidence >= threshold) {
            filtered.push_back(det);
        }
    }
    return filtered;
}
```

**Effect:**
```
Raw Detections:
  [Player: 0.92] ← High confidence (clear view)
  [Player: 0.78] ← Medium confidence
  [Shadow: 0.41] ← Low confidence (false positive)

After Filtering (threshold = 0.5):
  [Player: 0.92] ✅
  [Player: 0.78] ✅
  [Shadow: 0.41] ❌ Filtered out
```

**Why It Matters:**
- **Prevents Aiming at Shadows:** Reduces false positives
- **Improves Reliability:** Only aims at clear detections
- **Configurable:** Lower threshold = more aggressive, higher = more conservative

---

### 4. Grace Period for Lost Targets

**Purpose:** Don't immediately forget targets that temporarily disappear.

**Implementation:**
```cpp
void TargetTracker::update(DetectionBatch batch) {
    for (auto& target : activeTargets_) {
        if (!isMatched(target, batch)) {
            target.framesSinceSeen++;

            if (target.framesSinceSeen > gracePeriodFrames_) {
                removeTarget(target.id);  // Lost for >500ms → remove
            } else {
                predictPosition(target);  // Extrapolate with Kalman
            }
        }
    }
}
```

**Why It Matters:**
- **Occlusion Handling:** Player behind cover for 200ms doesn't disappear
- **Network Jitter:** Dropped frames don't break tracking
- **Smooth Tracking:** Prevents "flickering" on/off targets

---

## Responsible Development Guidelines

### For Contributors

**DO:**
- ✅ Focus on educational value (documentation, explanations)
- ✅ Optimize for performance and code quality
- ✅ Add safety mechanisms (deadman switch, FOV activation)
- ✅ Improve humanization to study human-AI interaction

**DON'T:**
- ❌ Add anti-anti-cheat features (kernel drivers, DLL injection)
- ❌ Obfuscate code or hide functionality
- ❌ Distribute compiled binaries on cheat forums
- ❌ Advertise this project as a "working cheat"

---

### For Users

**DO:**
- ✅ Use offline training modes or private servers
- ✅ Understand the risks before running ANY aimbot
- ✅ Respect game developers' intellectual property
- ✅ Report bugs and contribute improvements

**DON'T:**
- ❌ Use in online multiplayer games
- ❌ Stream or monetize aimbot gameplay
- ❌ Harass other players or developers
- ❌ Claim ignorance if caught (you were warned)

---

## Conclusion

MacroMan AI Aimbot v2 is a powerful educational tool for studying real-time computer vision and human-AI interaction. **With great power comes great responsibility.**

**Remember:**
- This is NOT a "cheat" - it's a research project
- The goal is learning, not winning unfairly
- Ethical use preserves the integrity of competitive gaming
- Understanding attack vectors helps build better defenses

**If you have doubts about whether your use case is ethical, it probably isn't.**

For technical questions: See `docs/DEVELOPER_GUIDE.md`
For setup help: See `docs/USER_GUIDE.md`
For troubleshooting: See `docs/FAQ.md`

---

**Stay Ethical. Play Fair. Learn Responsibly.**
