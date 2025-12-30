# Risk Mitigation & Anti-Detection Strategies

This directory contains architectural guidance for building **low-profile, external applications** that minimize detection surface. These documents are **strictly educational** and intended to understand modern anti-cheat systems and defensive programming practices.

**⚠️ IMPORTANT:** These techniques are documented for:
- **Educational purposes** - Understanding anti-cheat mechanisms
- **Authorized security testing** - Pentesting, CTF competitions
- **Defensive use cases** - Building detection-resistant tooling
- **Research purposes** - Academic study of user-mode security

**NOT intended for:**
- Competitive multiplayer games
- Unauthorized modification of software
- Circumventing legitimate security measures

---

## Documents

### 1. [Hardware Input Emulation](./01-hardware-input-emulation.md)
**The "Air-Gap" Input Method**

Why Windows API input injection (SendInput, mouse_event) is detectable and how hardware-level spoofing via microcontrollers provides indistinguishable input.

**Topics:**
- Arduino Leonardo/Pro Micro USB HID emulation
- KMBox and commercial hardware solutions
- Serial protocol design for low-latency input
- USB polling rate variance simulation

---

### 2. [Statistical Anomaly Avoidance](./02-statistical-anomaly-avoidance.md)
**Behavioral Heuristics & Server-Side Detection**

Modern anti-cheats (FairFight, VACNet, Ricochet) use machine learning to detect statistically impossible human behavior. This guide covers how to model realistic player behavior.

**Topics:**
- Dynamic bone selection (head/chest/neck switching)
- Fitts' Law-derived movement curves
- Variable input sampling (avoiding perfect 1000Hz)
- Miss simulation and natural inaccuracy
- Reaction time variance per scenario

---

### 3. [Process Hygiene](./03-process-hygiene.md)
**Digital Fingerprint Minimization**

Your executable itself is a detection vector. This guide covers how to make your binary appear benign to heuristic scanners.

**Topics:**
- Code signing strategies (self-signed vs commercial certs)
- String sanitization and symbol stripping
- Manifest hygiene (avoid unnecessary privileges)
- Version info spoofing
- Import table minimization
- Build reproducibility and polymorphism

---

### 4. [Read-Only Philosophy](./04-read-only-philosophy.md)
**Zero Foreign Memory Access**

The safest external application never touches the game process. This guide explains the "read-only" architecture and why memory scanning is a trap.

**Topics:**
- Screen capture vs memory reading
- External analysis only (vision-based approach)
- Avoiding PROCESS_VM_READ/PROCESS_VM_WRITE
- Why DLL injection is detectable
- Benefits of external overlay vs in-process hooks

---

### 5. [Advanced Obfuscation Techniques](./05-advanced-obfuscation.md) *(Post-MVP)*
**Beyond Basic String Encryption**

Advanced techniques for evading static and dynamic analysis. **Out of MVP scope** but documented for future reference.

**Topics:**
- Control flow flattening
- Virtual machine obfuscation (VMProtect-style)
- API hashing and dynamic resolution
- Encrypted IAT (Import Address Table)
- Anti-debugging techniques

---

## Architectural Principles

### The Four Pillars of Low-Profile Design

```
┌─────────────────────────────────────────────────────────┐
│  1. Hardware-Level Input                                │
│     • USB HID emulation (Arduino/KMBox)                 │
│     • Indistinguishable from real mouse                 │
│     • No Windows API hooks detectable                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  2. Statistical Realism                                 │
│     • Fitts' Law movement curves                        │
│     • Dynamic target selection                          │
│     • Natural variance in all parameters                │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  3. Binary Hygiene                                      │
│     • Code signing                                      │
│     • String encryption                                 │
│     • Benign manifest                                   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  4. Read-Only Operation                                 │
│     • Zero game memory access                           │
│     • Vision-based analysis only                        │
│     • External overlay (WDA_EXCLUDEFROMCAPTURE)         │
└─────────────────────────────────────────────────────────┘
```

---

## Detection Vectors & Mitigations

| Detection Vector | Risk Level | Mitigation | MVP Status |
|------------------|------------|------------|------------|
| **Windows API Input (SendInput)** | 🔴 High | Hardware USB HID emulation | ✅ ArduinoDriver |
| **Perfect Aim Tracking** | 🔴 High | Fitts' Law + overshoot | ✅ TrajectoryPlanner |
| **Constant Update Rate** | 🟡 Medium | Input timing variance | ✅ InputThread jitter |
| **Unsigned Binary** | 🟡 Medium | Code signing | 🔲 Post-MVP |
| **Suspicious Strings** | 🟡 Medium | Compile-time encryption | ✅ Phase 1 |
| **Overlay Visibility** | 🟡 Medium | SetWindowDisplayAffinity | ✅ Phase 6 |
| **Memory Scanning** | 🔴 High | Read-only (no game access) | ✅ Architecture |
| **DLL Injection** | 🔴 High | External process only | ✅ Architecture |
| **Kernel-Mode Driver** | 🔴 Critical | N/A (out of scope) | ❌ Not applicable |

**Risk Levels:**
- 🔴 **High:** Immediate ban if detected
- 🟡 **Medium:** Flagged for review, may lead to ban
- 🟢 **Low:** Logged but rarely actioned

---

## Implementation Integration

These strategies are integrated into the main architecture design document:

**From Design Doc:**
- **Section 10: Safety & Humanization** - Behavioral realism
- **Section 11: Critical Risks & Roadblocks** - Implementation traps

**This Section (Risk Mitigation):**
- **Deep dives** into each pillar
- **Code examples** for hardware integration
- **Statistical models** for human behavior
- **Binary hardening** techniques

---

## Comparison: MVP vs Production Hardening

| Feature | MVP (Educational) | Production (Hardened) |
|---------|-------------------|----------------------|
| **Input Method** | Win32Driver + ArduinoDriver | ArduinoDriver only (hardware) |
| **String Obfuscation** | Basic XOR encryption | skCrypter + dynamic decryption |
| **Binary Signing** | Unsigned | Code-signed certificate |
| **Overlay** | WDA_EXCLUDEFROMCAPTURE | Kernel-mode display filter |
| **Aim Behavior** | Bezier + tremor | Fitts' Law + dynamic bone selection |
| **Update Timing** | ±20% jitter | Realistic USB poll variance |
| **Memory Access** | Zero (read-only) | Zero (read-only) |

**MVP focuses on:**
- Correct architecture (read-only, external)
- Basic humanization (tremor, overshoot)
- Educational understanding

**Production would add:**
- Commercial-grade obfuscation
- Advanced statistical modeling
- Kernel-mode components (if legally permitted)

---

## Anti-Cheat Landscape (2025)

**Modern Anti-Cheat Systems:**

| System | Games | Detection Methods | Kernel-Mode |
|--------|-------|-------------------|-------------|
| **Vanguard** | Valorant | Memory scanning, driver validation, AI behavior | ✅ Yes |
| **Easy Anti-Cheat** | Apex, Fortnite | Screenshot capture, integrity checks | ✅ Yes |
| **BattlEye** | R6 Siege, PUBG | Process scanning, network analysis | ✅ Yes |
| **Ricochet** | Warzone, MW3 | Server-side ML, kernel driver | ✅ Yes |
| **VACNet** | CS2 | Statistical analysis, neural networks | ❌ No (user-mode) |
| **FairFight** | Battlefield | Pure statistical heuristics | ❌ No (server-side) |

**Trend:** Shift from client-side detection to **server-side behavioral analysis**. This means:
- Evasion at the binary level is less effective
- **Behavioral realism is now the primary defense**
- Statistical models (Fitts' Law, reaction time) are critical

---

## Legal & Ethical Considerations

### When This Is Appropriate:

✅ **Educational Research:**
- Understanding anti-cheat mechanisms
- Computer vision and AI research
- User-mode security research

✅ **Authorized Testing:**
- Penetration testing with explicit permission
- CTF (Capture The Flag) competitions
- Security training environments

✅ **Defensive Use:**
- Accessibility tools for disabled gamers (with developer permission)
- Testing your own software's resilience
- Building detection-resistant legitimate tooling

### When This Is Inappropriate:

❌ **Competitive Multiplayer Games:**
- Online ranked matches
- Esports competitions
- Any game with an active player base

❌ **Commercial Cheating:**
- Selling or distributing for profit
- Subscription-based cheat services
- Undermining game integrity

❌ **Unauthorized Modification:**
- Violating Terms of Service
- Circumventing DRM or anti-tamper
- Modifying game files

---

## Recommended Reading Order

**For beginners:**
1. Start with the [main design document](../plans/2025-12-29-modern-aimbot-architecture-design.md)
2. Read [Read-Only Philosophy](./04-read-only-philosophy.md) to understand the "why"
3. Review [Statistical Anomaly Avoidance](./02-statistical-anomaly-avoidance.md) for behavioral realism

**For implementers:**
1. [Hardware Input Emulation](./01-hardware-input-emulation.md) for ArduinoDriver implementation
2. [Process Hygiene](./03-process-hygiene.md) for binary hardening
3. [Advanced Obfuscation](./05-advanced-obfuscation.md) for post-MVP enhancements

**For security researchers:**
1. All documents in order (01-05)
2. Cross-reference with main design document Section 10-11
3. Review anti-cheat landscape to understand current state-of-the-art

---

## Tools & Libraries

**Hardware:**
- [Arduino Leonardo](https://store.arduino.cc/arduino-leonardo-with-headers) - USB HID emulation
- [Teensy 3.2+](https://www.pjrc.com/teensy/) - Faster HID processing
- [KMBox](https://kmboxnet.com/) - Commercial mouse emulation box

**Software:**
- [skCrypter](https://github.com/skadro-official/skCrypter) - String obfuscation
- [VMProtect](https://vmpsoft.com/) - Commercial packer (expensive, overkill for MVP)
- [UPX](https://upx.github.io/) - Open-source packer (basic compression)

**Analysis Tools:**
- [Process Hacker](https://processhacker.sourceforge.io/) - Process inspection
- [API Monitor](http://www.rohitab.com/apimonitor) - Windows API call logging
- [Sysinternals Suite](https://docs.microsoft.com/sysinternals/) - Process Explorer, Process Monitor

---

## Contributing

If you have additional risk mitigation strategies or updates to anti-cheat landscapes, please submit a pull request with:

1. **Evidence-based claims** (link to research papers, blog posts, or public documentation)
2. **Code examples** (if applicable)
3. **Clear categorization** (which pillar does it address?)

**DO NOT submit:**
- Kernel-mode anti-cheat bypass techniques (out of scope)
- Game-specific exploits or vulnerabilities
- Commercial cheat service information

---

## Disclaimer

This documentation is provided for **educational and research purposes only**. The authors do not condone:

- Using these techniques for competitive advantage in online games
- Violating Terms of Service of any software
- Distributing or selling cheat software

Use of these techniques is at your own risk. Many jurisdictions have laws against circumventing technical protection measures. Always obtain explicit permission before testing against any system you do not own.

---

**Last Updated:** 2025-12-29
**Version:** 1.0
**Maintainer:** Macroman AI Aimbot Project

---

*This is part of the Macroman AI Aimbot educational project. See [main design document](../plans/2025-12-29-modern-aimbot-architecture-design.md) for full architecture.*
