# Statistical Anomaly Avoidance - Behavioral Heuristics

**Pillar 2 of Low-Profile Design**

---

## Table of Contents

1. [The Rise of Server-Side Detection](#the-rise-of-server-side-detection)
2. [Fitts' Law and Human Movement](#fitts-law-and-human-movement)
3. [Dynamic Bone Selection](#dynamic-bone-selection)
4. [Natural Inaccuracy & Miss Simulation](#natural-inaccuracy--miss-simulation)
5. [Reaction Time Variance](#reaction-time-variance)
6. [Variable Input Sampling](#variable-input-sampling)
7. [Integration with TrajectoryPlanner](#integration-with-trajectoryplanner)

---

## The Rise of Server-Side Detection

### Why Client-Side Evasion Isn't Enough

**Modern anti-cheat systems have shifted focus:**

| Era | Detection Method | Evasion Strategy |
|-----|------------------|------------------|
| **2010-2015** | Memory scanning, API hooks | Obfuscation, kernel drivers |
| **2015-2020** | Signature scanning, integrity checks | Polymorphism, anti-debugging |
| **2020-2025** | **Server-side behavioral analysis** | **Statistical realism** |

**Examples of Server-Side Systems:**

#### 1. **VACNet (Valve)**
- Machine learning model trained on human gameplay
- Analyzes crosshair placement, movement patterns, reaction times
- Flags statistically improbable behaviors

#### 2. **FairFight (GameBlanco)**
- No client-side component (all server-side)
- Compares player stats to population averages
- Detects outliers: too-high headshot %, inhuman reaction times

#### 3. **Ricochet (Activision)**
- Hybrid: kernel driver + server-side ML
- Analyzes aim smoothness, tracking consistency
- Detects "snap" behavior (instant target acquisition)

**Key Insight:** Even if your binary is undetectable, **your behavior will get you banned**.

---

## Fitts' Law and Human Movement

### The Science of Human Motor Control

**Fitts' Law (1954):** The time to acquire a target is logarithmically proportional to distance and inversely proportional to target size.

```
T = a + b * log₂(D / W + 1)
```

Where:
- `T` = Time to reach target (ms)
- `D` = Distance to target (pixels)
- `W` = Width of target (pixels)
- `a, b` = Empirically determined constants

**For FPS games (typical values):**
- `a = 50ms` (base movement initiation time)
- `b = 150ms` (scaling factor)

**Example:**
```cpp
// Target 300 pixels away, 50 pixels wide (torso)
T = 50 + 150 * log₂(300 / 50 + 1)
  = 50 + 150 * log₂(7)
  = 50 + 150 * 2.807
  = 50 + 421
  = 471ms
```

**Takeaway:** Humans cannot instantly snap to a target 300 pixels away. It takes ~470ms.

### Velocity Profile (Submovement Model)

**Human movement is not constant velocity:**

```
      Velocity
         ↑
         │        ╱╲
    Peak │       ╱  ╲
         │      ╱    ╲
         │     ╱      ╲_____ (settle)
         │____╱
         └────────────────→ Time
         Start   Peak   End
```

**Phases:**
1. **Acceleration** (0-30%): Rapid increase to peak velocity
2. **Deceleration** (30-90%): Gradual slowdown as approaching target
3. **Settling** (90-100%): Micro-adjustments, tremor

**Typical velocity curve:**
```cpp
float getFittsVelocity(float t) {
    // t = progress [0, 1]
    if (t < 0.3f) {
        // Acceleration phase: quadratic
        return (t / 0.3f) * (t / 0.3f);  // 0 → 1
    } else if (t < 0.9f) {
        // Deceleration phase: inverse quadratic
        float localT = (t - 0.3f) / 0.6f;  // 0 → 1
        return 1.0f - localT * localT * 0.5f;  // 1 → 0.5
    } else {
        // Settling phase: slow micro-adjustments
        return 0.1f + 0.05f * std::sin((t - 0.9f) * 20.0f);  // Jitter
    }
}
```

**Integration with Bezier:**
```cpp
Vec2 TrajectoryPlanner::stepWithFittsLaw(float t, const Vec2& start, const Vec2& target) {
    // Standard Bezier curve
    Vec2 pos = bezierCurve.evaluate(t);

    // Modulate velocity based on Fitts' Law
    float velocity = getFittsVelocity(t);

    // Apply velocity scaling
    Vec2 direction = (target - start).normalized();
    return start + direction * (target - start).length() * t * velocity;
}
```

---

## Dynamic Bone Selection

### The Problem with Perfect Headshots

**Aimbot Signature:**
- 100% headshot rate
- Never aims at body when head is available
- Perfect bone locking (crosshair glued to skull)

**Professional Players:**
- Headshot rate: 20-50% (game-dependent)
- Aim at center mass (chest) when:
  - Target is far away (head is small)
  - Target is moving erratically
  - Spraying/burst fire (recoil control easier on chest)
- Switch to head only for:
  - Stationary targets
  - Close range
  - Tap-firing

### Implementation: Dynamic Bone Selector

```cpp
enum class HitboxType {
    Head,
    Neck,
    UpperChest,
    LowerChest,
    Stomach
};

class DynamicBoneSelector {
    std::mt19937 rng;
    std::uniform_real_distribution<float> chance{0.0f, 1.0f};

public:
    HitboxType selectBone(const Target& target, float distanceToTarget) {
        // Calculate headshot probability based on distance
        float headshotChance = calculateHeadshotChance(distanceToTarget);

        if (chance(rng) < headshotChance) {
            return HitboxType::Head;
        } else {
            // Weighted selection of body parts
            float rand = chance(rng);
            if (rand < 0.4f) return HitboxType::UpperChest;
            else if (rand < 0.7f) return HitboxType::Neck;
            else if (rand < 0.9f) return HitboxType::LowerChest;
            else return HitboxType::Stomach;
        }
    }

private:
    float calculateHeadshotChance(float distance) {
        // Close range (< 10m): 70% headshot
        // Medium range (10-30m): 40% headshot
        // Long range (> 30m): 15% headshot

        if (distance < 10.0f) {
            return 0.7f;
        } else if (distance < 30.0f) {
            return 0.4f - (distance - 10.0f) / 20.0f * 0.25f;  // Linear interpolation
        } else {
            return 0.15f;
        }
    }
};
```

**Usage in Tracking Thread:**
```cpp
void selectBestTarget() {
    Target best = findClosestTarget();

    float distance = calculateDistance(player.position, best.position);
    HitboxType bone = boneSelector.selectBone(best, distance);

    AimCommand cmd;
    cmd.targetPosition = best.getBonePosition(bone);  // Not always head!
    cmd.hasTarget = true;
}
```

**Result:** Headshot rate varies naturally based on engagement distance, matching human players.

---

## Natural Inaccuracy & Miss Simulation

### Humans Miss. Aimbots Don't.

**The Problem:**
- Perfect tracking (crosshair never leaves hitbox)
- 100% hit rate in spray patterns
- No "whiff" shots

**Solution: Intentional Miss Probability**

```cpp
class MissSimulator {
    std::mt19937 rng;
    std::normal_distribution<float> offsetDist{0.0f, 5.0f};  // μ=0, σ=5 pixels

public:
    Vec2 applyInaccuracy(Vec2 targetPos, float targetVelocity) {
        // Higher velocity = higher miss chance
        float missChance = std::min(targetVelocity / 100.0f, 0.3f);  // Max 30%

        if (std::uniform_real_distribution<float>(0, 1)(rng) < missChance) {
            // Apply Gaussian offset
            float offsetX = offsetDist(rng);
            float offsetY = offsetDist(rng);
            return {targetPos.x + offsetX, targetPos.y + offsetY};
        }

        return targetPos;  // No miss
    }
};
```

**Integration:**
```cpp
Vec2 TrajectoryPlanner::plan(const AimCommand& cmd) {
    Vec2 targetPos = cmd.targetPosition;

    // Apply inaccuracy based on target movement
    targetPos = missSimulator.applyInaccuracy(targetPos, cmd.targetVelocity.length());

    // Plan trajectory to possibly-offset target
    return planBezierCurve(currentPos, targetPos);
}
```

**Result:** ~5-15% miss rate on moving targets, matching human performance.

---

## Reaction Time Variance

### Context-Dependent Reaction Times

**Human reaction times vary based on context:**

| Scenario | Reaction Time | Standard Deviation |
|----------|---------------|-------------------|
| **Expected target** (pre-aiming corner) | 150-200ms | ±30ms |
| **Unexpected target** (flank) | 200-300ms | ±50ms |
| **Fatigued** (2+ hours gameplay) | 250-400ms | ±80ms |
| **High stress** (1v5 clutch) | 180-250ms | ±40ms |

**Implementation:**
```cpp
class ReactionTimeManager {
    std::mt19937 rng;
    float fatigueLevel{0.0f};  // 0 = fresh, 1 = exhausted

public:
    float getReactionDelay(bool wasExpected, bool isStressed) {
        float baseDelay = wasExpected ? 160.0f : 250.0f;
        float stdDev = wasExpected ? 25.0f : 50.0f;

        // Fatigue increases both delay and variance
        baseDelay += fatigueLevel * 100.0f;
        stdDev += fatigueLevel * 30.0f;

        // Stress slightly reduces delay
        if (isStressed) {
            baseDelay *= 0.9f;
        }

        std::normal_distribution<float> dist(baseDelay, stdDev);
        return std::clamp(dist(rng), 100.0f, 500.0f);
    }

    void updateFatigue(float sessionDuration) {
        // Linearly increase fatigue over 3 hours
        fatigueLevel = std::min(sessionDuration / (3.0f * 3600.0f), 1.0f);
    }
};
```

**Usage:**
```cpp
void onTargetDetected(const Target& target) {
    bool wasExpected = crosshairNearTarget(target);  // Were we pre-aiming?
    bool isStressed = playerHealth < 50;  // Low health = high stress

    float delay = reactionTime.getReactionDelay(wasExpected, isStressed);

    // Buffer detection for reaction delay
    scheduleAimCommand(target, delay);
}
```

**Result:** Reaction times vary 150-400ms based on scenario, matching human behavior.

---

## Variable Input Sampling

### Avoiding Perfect 1000Hz

**Already covered in [Hardware Input Emulation](./01-hardware-input-emulation.md)**, but worth repeating:

**Bad:**
```cpp
void inputLoop() {
    while (running) {
        processInput();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // ⚠️ Perfect 1000Hz
    }
}
```

**Good:**
```cpp
void inputLoop() {
    std::mt19937 rng;
    std::uniform_real_distribution<float> jitter(0.8f, 1.2f);

    while (running) {
        processInput();

        float sleepTime = 1.0f * jitter(rng);  // 0.8ms - 1.2ms
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(sleepTime));
    }
}
```

**Result:** Input rate varies 800-1200 Hz, matching real hardware USB poll variance.

---

## Integration with TrajectoryPlanner

### Putting It All Together

**Enhanced TrajectoryPlanner:**

```cpp
class TrajectoryPlanner {
    BezierCurve bezierCurve;
    OneEuroFilter filter;
    DynamicBoneSelector boneSelector;
    MissSimulator missSimulator;
    HumanTremorSimulator tremorSimulator;

public:
    Vec2 step(const AimCommand& cmd, float dt) {
        // 1. Select target bone (not always head)
        Vec2 targetPos = cmd.targetPosition;

        // 2. Apply intentional inaccuracy (miss simulation)
        targetPos = missSimulator.applyInaccuracy(targetPos, cmd.targetVelocity.length());

        // 3. Plan Fitts' Law-based trajectory
        float t = bezierProgress;  // [0, 1]
        float velocity = getFittsVelocity(t);
        Vec2 movement = bezierCurve.evaluate(t) * velocity;

        // 4. Apply human tremor
        movement = tremorSimulator.applyTremor(movement, dt);

        // 5. Apply 1 Euro filter (smoothing)
        movement = filter.filter(movement, dt);

        // 6. Update progress
        bezierProgress += dt * 2.0f;  // Adjust speed

        return movement;
    }
};
```

**Flow:**
```
Target Detected
      ↓
[Reaction Delay] (150-400ms)
      ↓
[Bone Selection] (Head 40% / Chest 50% / Other 10%)
      ↓
[Miss Simulation] (5-15% offset on moving targets)
      ↓
[Fitts' Law Trajectory] (Realistic velocity curve)
      ↓
[Tremor + 1 Euro Filter] (Micro-jitter + smoothing)
      ↓
Mouse Movement (800-1200 Hz variance)
```

---

## Statistical Validation

### How to Test Your Behavioral Realism

**1. Record Gameplay Session (30 minutes)**

**2. Analyze Statistics:**
```python
import numpy as np

# Extract stats from replay
headshot_rate = num_headshots / total_kills
avg_reaction_time = np.mean(reaction_times)
reaction_time_stddev = np.std(reaction_times)
tracking_smoothness = calculate_jerk(mouse_positions)  # Third derivative
flick_accuracy = close_range_hits / close_range_attempts

print(f"Headshot Rate: {headshot_rate:.1%}")  # Should be 20-50%
print(f"Avg Reaction: {avg_reaction_time:.0f}ms")  # Should be 150-300ms
print(f"Reaction StdDev: {reaction_time_stddev:.0f}ms")  # Should be 30-80ms
print(f"Flick Accuracy: {flick_accuracy:.1%}")  # Should be 60-85%
```

**3. Compare to Pro Players:**
```
Metric               | You    | Pro Average | Verdict
---------------------|--------|-------------|----------
Headshot Rate        | 45%    | 40%         | ✅ Normal
Avg Reaction         | 185ms  | 180ms       | ✅ Normal
Reaction StdDev      | 35ms   | 40ms        | ✅ Normal
Tracking Smoothness  | 0.02   | 0.03        | ⚠️ Too smooth (add tremor)
Flick Accuracy       | 95%    | 70%         | 🔴 TOO HIGH (add miss simulation)
```

**Adjust parameters until all metrics are within human range.**

---

## Advanced: Machine Learning Evasion

**Future Direction (Post-MVP):**

Train a **Generative Adversarial Network (GAN)** to produce human-like mouse trajectories:

```
┌─────────────────┐          ┌───────────────────┐
│   Generator     │  Fake    │  Discriminator    │
│  (Your Aimbot)  │ ───────→ │  (Anti-Cheat ML)  │
└─────────────────┘          └───────────────────┘
         ↑                             │
         │         Feedback            │
         └─────────────────────────────┘
```

**Concept:**
- Generator produces mouse trajectories
- Discriminator (trained on real player data) tries to distinguish fake from real
- Generator improves until discriminator cannot tell the difference

**Implementation:** Beyond scope of this project, but conceptually feasible.

---

## Conclusion

**Statistical realism is now more important than binary obfuscation.**

**Key Takeaways:**
- Server-side detection analyzes your behavior, not your binary
- Fitts' Law provides the foundation for realistic movement curves
- Dynamic bone selection prevents 100% headshot rates
- Miss simulation adds natural inaccuracy
- Reaction time must vary based on context (expected vs unexpected)
- Input timing should have ±20% jitter

**Checklist for Behavioral Realism:**
- [ ] Headshot rate < 60% (preferably 30-50%)
- [ ] Reaction times vary 150-400ms based on context
- [ ] Miss rate 5-15% on moving targets
- [ ] Aim at chest/neck 40-60% of the time
- [ ] Fitts' Law velocity profile (not constant speed)
- [ ] Tremor simulation (8-12 Hz jitter)
- [ ] Input rate variance (800-1200 Hz)

**Next Steps:**
1. Implement DynamicBoneSelector
2. Add MissSimulator to TrajectoryPlanner
3. Integrate ReactionTimeManager
4. Record 30-minute session and validate stats

**Related Documents:**
- [Hardware Input Emulation](./01-hardware-input-emulation.md) - Input variance
- [Main Architecture](../plans/2025-12-29-modern-aimbot-architecture-design.md) - Section 10: Safety & Humanization

---

**Last Updated:** 2025-12-29
