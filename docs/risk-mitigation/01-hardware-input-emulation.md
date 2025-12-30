# Hardware Input Emulation - The "Air-Gap" Input Method

**Pillar 1 of Low-Profile Design**

---

## Table of Contents

1. [The Problem with Windows API Input](#the-problem-with-windows-api-input)
2. [Hardware-Level Input Solution](#hardware-level-input-solution)
3. [Arduino Implementation](#arduino-implementation)
4. [Serial Protocol Design](#serial-protocol-design)
5. [USB Polling Rate Variance](#usb-polling-rate-variance)
6. [Commercial Solutions](#commercial-solutions)
7. [Integration with Main Architecture](#integration-with-main-architecture)

---

## The Problem with Windows API Input

### Why SendInput/mouse_event Is Detectable

**Windows provides several APIs for synthetic input:**
- `SendInput()` (recommended, modern)
- `mouse_event()` (deprecated, legacy)
- `keybd_event()` (keyboard input)

**Detection Methods:**

#### 1. **API Hooking**
```cpp
// Anti-cheat installs hooks on SendInput
UINT WINAPI HookedSendInput(UINT nInputs, LPINPUT pInputs, int cbSize) {
    // Log synthetic input
    LogSuspiciousActivity("SendInput called by external process");

    // Call original
    return RealSendInput(nInputs, pInputs, cbSize);
}
```

**Detection Rate:** Very high. Most kernel-mode anti-cheats hook these APIs.

#### 2. **Timing Analysis**
```cpp
// SendInput has perfect timing (no jitter)
// Real hardware has USB poll variance
if (input_interval_stddev < threshold) {
    FlagAsSynthetic();
}
```

#### 3. **Missing Hardware Events**
- Real mouse movement generates WM_INPUT messages with raw HID data
- SendInput only generates WM_MOUSEMOVE (higher-level)
- Anti-cheat can detect this discrepancy

**Example Detection:**
```cpp
// Valorant's Vanguard checks for:
bool IsSyntheticInput(const INPUT& input) {
    // Check if corresponding raw input exists
    if (!HasMatchingRawInput(input.timestamp)) {
        return true;  // Synthetic
    }

    // Check for perfect timing
    if (InputTimingIsTooConsistent()) {
        return true;
    }

    return false;
}
```

---

## Hardware-Level Input Solution

### The "Air-Gap" Concept

**Idea:** Instead of injecting input via Windows API, **physically emulate a USB mouse** using a microcontroller.

```
┌──────────────┐           ┌─────────────┐          ┌────────────┐
│  Aimbot PC   │  Serial   │  Arduino    │   USB    │  Gaming PC │
│  (Software)  │ ───────→  │  (Hardware) │ ───────→ │  (Target)  │
└──────────────┘   (COM3)  └─────────────┘  (Mouse) └────────────┘
                              USB HID
                            Emulation
```

**Why This Works:**
1. **Gaming PC sees a real USB device** (Vendor ID, Product ID, HID descriptors)
2. **Indistinguishable from a Logitech/Razer mouse** at the USB protocol level
3. **No Windows API calls** on the gaming PC
4. **Generates proper raw input (WM_INPUT)** like physical hardware

---

## Arduino Implementation

### Supported Hardware

| Device | Chip | USB HID | Speed | Cost | Recommended |
|--------|------|---------|-------|------|-------------|
| **Arduino Leonardo** | ATmega32U4 | ✅ Native | 16 MHz | $25 | ✅ Best for beginners |
| **Arduino Pro Micro** | ATmega32U4 | ✅ Native | 16 MHz | $10 | ✅ Compact, cheap |
| **Teensy 3.2** | MK20DX256 | ✅ Native | 72 MHz | $20 | ✅ Faster processing |
| **Teensy 4.0** | IMXRT1062 | ✅ Native | 600 MHz | $20 | ⚠️ Overkill |
| **Arduino Uno** | ATmega328P | ❌ No | 16 MHz | $25 | ❌ Cannot emulate HID |

**Why ATmega32U4?**
- Built-in USB controller (can emulate HID devices)
- Arduino libraries make it easy (`Mouse.h`, `Keyboard.h`)
- Widely available and cheap

### Basic Arduino Sketch

**Upload this to Arduino Leonardo/Pro Micro:**

```cpp
// Arduino sketch for USB HID mouse emulation
#include <Mouse.h>

struct MouseCommand {
    int16_t dx;  // X movement (relative)
    int16_t dy;  // Y movement (relative)
    uint8_t buttons;  // Button state (optional)
};

void setup() {
    Serial.begin(115200);  // Serial communication with PC
    Mouse.begin();         // Initialize USB HID mouse
}

void loop() {
    if (Serial.available() >= sizeof(MouseCommand)) {
        MouseCommand cmd;
        Serial.readBytes((char*)&cmd, sizeof(cmd));

        // Move mouse (sends USB HID report)
        Mouse.move(cmd.dx, cmd.dy, 0);

        // Optional: Handle buttons
        if (cmd.buttons & 0x01) Mouse.press(MOUSE_LEFT);
        else Mouse.release(MOUSE_LEFT);
    }
}
```

**Latency:** ~1-2ms (USB poll rate dependent)

---

## Serial Protocol Design

### High-Speed Serial Communication

**ArduinoDriver Implementation (C++ side):**

```cpp
class ArduinoDriver : public IMouseDriver {
    serial::Serial port;

public:
    void initialize() override {
        // Open COM port at high baud rate
        port.setPort("COM3");
        port.setBaudrate(115200);  // Or higher: 230400, 460800
        port.setTimeout(serial::Timeout::max(), 0, 0, 0, 0);
        port.open();

        if (!port.isOpen()) {
            throw std::runtime_error("Failed to open Arduino serial port");
        }

        LOG_INFO("ArduinoDriver initialized on COM3 @ 115200 baud");
    }

    void move(int dx, int dy) override {
        MouseCommand cmd;
        cmd.dx = static_cast<int16_t>(std::clamp(dx, -127, 127));
        cmd.dy = static_cast<int16_t>(std::clamp(dy, -127, 127));
        cmd.buttons = 0;  // No button changes

        // Send binary command
        port.write((uint8_t*)&cmd, sizeof(cmd));
    }

    void shutdown() override {
        if (port.isOpen()) {
            port.close();
        }
    }
};
```

**Protocol Characteristics:**
- **Binary format** (not text) for efficiency
- **Fixed-size packets** (6 bytes: dx=2, dy=2, buttons=1, checksum=1)
- **No acknowledgment** (fire-and-forget for low latency)
- **Optional checksum** for reliability

### Advanced Protocol with Checksum

```cpp
struct MouseCommand {
    int16_t dx;
    int16_t dy;
    uint8_t buttons;
    uint8_t checksum;  // XOR of all bytes

    void calculateChecksum() {
        checksum = 0;
        uint8_t* bytes = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(*this) - 1; ++i) {
            checksum ^= bytes[i];
        }
    }

    bool validateChecksum() const {
        uint8_t calc = 0;
        uint8_t* bytes = (uint8_t*)this;
        for (size_t i = 0; i < sizeof(*this) - 1; ++i) {
            calc ^= bytes[i];
        }
        return calc == checksum;
    }
};
```

**Arduino side validation:**
```cpp
void loop() {
    if (Serial.available() >= sizeof(MouseCommand)) {
        MouseCommand cmd;
        Serial.readBytes((char*)&cmd, sizeof(cmd));

        if (cmd.validateChecksum()) {
            Mouse.move(cmd.dx, cmd.dy, 0);
        } else {
            // Drop corrupted packet
        }
    }
}
```

---

## USB Polling Rate Variance

### The Problem with Perfect Timing

**Real USB mice poll at 125Hz, 500Hz, or 1000Hz, but:**
- Not perfectly consistent (±0.5ms jitter due to USB timing)
- Varies based on system load
- Affected by USB controller buffering

**Synthetic perfect 1000Hz is detectable.**

### Adding Realistic Variance

**Arduino Implementation:**
```cpp
#include <Mouse.h>

uint32_t lastMoveTime = 0;
const uint16_t baseInterval = 1000;  // 1ms (1000Hz)

void setup() {
    Serial.begin(115200);
    Mouse.begin();
    randomSeed(analogRead(0));  // Seed RNG
}

void loop() {
    if (Serial.available() >= sizeof(MouseCommand)) {
        MouseCommand cmd;
        Serial.readBytes((char*)&cmd, sizeof(cmd));

        // Add jitter: ±20% variance
        uint32_t now = micros();
        uint16_t jitter = random(800, 1200);  // 0.8ms - 1.2ms

        if (now - lastMoveTime >= jitter) {
            Mouse.move(cmd.dx, cmd.dy, 0);
            lastMoveTime = now;
        } else {
            // Buffer command (or drop if stale)
        }
    }
}
```

**Result:** USB reports sent at variable intervals (800Hz - 1200Hz) matching real hardware variance.

---

## Commercial Solutions

### KMBox and Similar Devices

**KMBox:** Commercial mouse/keyboard emulation box ($150-300)

**Features:**
- Pre-configured USB HID emulation
- Multiple game profiles
- Network or serial control
- Built-in anti-detection features

**Pros:**
- Plug-and-play
- No Arduino programming needed
- Optimized for gaming

**Cons:**
- Expensive
- Closed-source (trust issue)
- May have unique fingerprint (detectable if blacklisted)

**Alternatives:**
- **Cronus Zen** ($100) - Primarily for controller emulation, can do mouse
- **Titan Two** ($100) - Similar to Cronus
- **DIY PCB solutions** - Custom boards with USB controller chips

**Recommendation for MVP:** Use Arduino Leonardo ($25) for educational purposes. Commercial solutions are for production deployment.

---

## Integration with Main Architecture

### Input Thread Modification

**Original (Win32Driver):**
```cpp
void inputLoop() {
    auto driver = std::make_unique<Win32Driver>();  // SendInput

    while (running) {
        AimCommand cmd = latestCommand.load();
        Vec2 movement = trajectoryPlanner.step(cmd);
        driver->move(movement.x, movement.y);  // ⚠️ Detectable
    }
}
```

**Modified (ArduinoDriver):**
```cpp
void inputLoop() {
    auto driver = std::make_unique<ArduinoDriver>("COM3");  // Hardware

    while (running) {
        AimCommand cmd = latestCommand.load();
        Vec2 movement = trajectoryPlanner.step(cmd);
        driver->move(movement.x, movement.y);  // ✅ USB HID, indistinguishable
    }
}
```

**Configuration (plugins.ini):**
```ini
[Input]
Driver=ArduinoDriver  # or Win32Driver for testing
Port=COM3
BaudRate=115200
```

### Factory Pattern Integration

```cpp
std::unique_ptr<IMouseDriver> createMouseDriver(const std::string& type) {
    if (type == "Arduino") {
        return std::make_unique<ArduinoDriver>("COM3");
    } else if (type == "Win32") {
        return std::make_unique<Win32Driver>();
    } else {
        throw std::invalid_argument("Unknown driver type");
    }
}
```

---

## Latency Comparison

| Method | Latency | Detection Risk | Cost |
|--------|---------|----------------|------|
| **SendInput** | <0.1ms | 🔴 Very High | $0 |
| **Arduino Leonardo** | 1-2ms | 🟢 Very Low | $25 |
| **Teensy 3.2** | 0.5-1ms | 🟢 Very Low | $20 |
| **KMBox** | 0.5-1ms | 🟡 Low-Medium | $200 |

**Verdict:** 1-2ms latency is acceptable for the **massive** gain in undetectability.

---

## Testing & Validation

### How to Verify It Works

**1. Check USB Device Manager:**
```powershell
Get-PnPDevice -Class Mouse | Format-Table -AutoSize
```

Should show Arduino as a HID-compliant mouse.

**2. Monitor Raw Input:**
```cpp
// In WndProc:
case WM_INPUT: {
    RAWINPUT raw;
    UINT size = sizeof(raw);
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

    if (raw.header.dwType == RIM_TYPEMOUSE) {
        LOG_DEBUG("Raw mouse: dx={}, dy={}", raw.data.mouse.lLastX, raw.data.mouse.lLastY);
    }
}
```

If you see raw input messages, the Arduino is successfully emulating USB HID.

**3. Test with Mouse Tester:**
Use [Mouse Tester](https://github.com/microe1/MouseTester) to verify:
- Polling rate (should vary 800-1200 Hz)
- Movement smoothness
- No dropped inputs

---

## Troubleshooting

**Problem:** Arduino not recognized as mouse.

**Solution:**
- Ensure you're using Leonardo/Pro Micro (has ATmega32U4)
- Upload sketch with `Mouse.begin()`
- Check Device Manager for "HID-compliant mouse"

**Problem:** Serial port permission denied.

**Solution:**
- Close Arduino IDE (it locks the COM port)
- Grant admin privileges to your aimbot executable
- Check port name (Windows: `COM3`, Linux: `/dev/ttyACM0`)

**Problem:** High latency (>5ms).

**Solution:**
- Increase baud rate: `Serial.begin(230400)`
- Use binary protocol (not text/JSON)
- Remove delays in Arduino `loop()`

---

## Advanced: Dual-Mode Support

**Support both Win32 and Arduino for development:**

```cpp
class HybridMouseDriver : public IMouseDriver {
    std::unique_ptr<IMouseDriver> primaryDriver;
    std::unique_ptr<IMouseDriver> fallbackDriver;

public:
    HybridMouseDriver() {
        // Try Arduino first
        try {
            primaryDriver = std::make_unique<ArduinoDriver>("COM3");
            LOG_INFO("Using ArduinoDriver");
        } catch (...) {
            // Fall back to Win32
            primaryDriver = std::make_unique<Win32Driver>();
            LOG_WARN("Arduino not available, falling back to Win32Driver");
        }
    }

    void move(int dx, int dy) override {
        primaryDriver->move(dx, dy);
    }
};
```

**Use case:** Development without Arduino, deploy with Arduino.

---

## Conclusion

**Hardware input emulation is the #1 defense against input detection.**

**Key Takeaways:**
- SendInput/mouse_event are trivially detectable via API hooks
- Arduino Leonardo/Pro Micro can emulate USB HID mouse for <$25
- Serial protocol should be binary, checksummed, and fire-and-forget
- Add ±20% jitter to USB report timing to match real hardware
- Integration is simple: swap Win32Driver for ArduinoDriver

**Next Steps:**
1. Purchase Arduino Leonardo ($25)
2. Upload the provided sketch
3. Test with Mouse Tester
4. Integrate ArduinoDriver into your project
5. Configure via `plugins.ini`

**Related Documents:**
- [Statistical Anomaly Avoidance](./02-statistical-anomaly-avoidance.md) - Behavioral realism
- [Main Architecture](../plans/2025-12-29-modern-aimbot-architecture-design.md) - Section 6: Input Module

---

**Last Updated:** 2025-12-29
