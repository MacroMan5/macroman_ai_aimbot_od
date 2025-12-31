# Arduino HID Driver Setup Guide

The **ArduinoDriver** provides hardware-level mouse emulation using an Arduino Leonardo or Pro Micro board. This method is undetectable by kernel-level anti-cheat systems since the input appears as a real USB HID device.

---

## Hardware Requirements

**Supported Boards:**
- Arduino Leonardo
- Arduino Pro Micro (ATmega32U4)
- Any ATmega32U4-based board with native USB support

**Why these boards?**
- ATmega32U4 has native USB HID capability
- Can emulate mouse/keyboard at hardware level
- Appears as legitimate USB device to OS

---

## Software Requirements

### 1. Install libserial (Windows)

**Option A: vcpkg (Recommended)**
```bash
# Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat

# Install libserial
vcpkg install serial:x64-windows

# Integrate with Visual Studio
vcpkg integrate install
```

**Option B: Manual Build**
```bash
# Clone libserial
git clone https://github.com/wjwwood/serial.git
cd serial

# Build with CMake
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=C:/libserial
cmake --build build --config Release
cmake --install build
```

### 2. Enable ArduinoDriver in CMake

```bash
# Configure with Arduino support
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_ARDUINO=ON

# If libserial is not in system path, specify manually:
cmake -B build -S . -DENABLE_ARDUINO=ON -Dserial_DIR="C:/libserial/lib/cmake/serial"

# Build
cmake --build build --config Release
```

---

## Arduino Firmware

### 1. Flash HID Firmware to Arduino

**Firmware Code** (`arduino_hid_mouse/arduino_hid_mouse.ino`):

```cpp
#include <Mouse.h>

void setup() {
  Serial.begin(115200);
  Mouse.begin();

  // Wait for serial connection
  while (!Serial);
  Serial.println("READY");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // Mouse movement: M,dx,dy
    if (cmd.startsWith("M,")) {
      int commaIdx = cmd.indexOf(',', 2);
      int dx = cmd.substring(2, commaIdx).toInt();
      int dy = cmd.substring(commaIdx + 1).toInt();
      Mouse.move(dx, dy, 0);
    }

    // Button press: PL, PR, PM, PS1, PS2
    else if (cmd.startsWith("P")) {
      if (cmd == "PL") Mouse.press(MOUSE_LEFT);
      else if (cmd == "PR") Mouse.press(MOUSE_RIGHT);
      else if (cmd == "PM") Mouse.press(MOUSE_MIDDLE);
    }

    // Button release: RL, RR, RM, RS1, RS2
    else if (cmd.startsWith("R")) {
      if (cmd == "RL") Mouse.release(MOUSE_LEFT);
      else if (cmd == "RR") Mouse.release(MOUSE_RIGHT);
      else if (cmd == "RM") Mouse.release(MOUSE_MIDDLE);
    }

    // Button click: CL, CR, CM, CS1, CS2
    else if (cmd.startsWith("C")) {
      if (cmd == "CL") Mouse.click(MOUSE_LEFT);
      else if (cmd == "CR") Mouse.click(MOUSE_RIGHT);
      else if (cmd == "CM") Mouse.click(MOUSE_MIDDLE);
    }

    // Hardware key queries: QA (aim), QS (shoot), QZ (zoom)
    // TODO: Implement physical button monitoring if needed
    else if (cmd == "QA") Serial.println("0");
    else if (cmd == "QS") Serial.println("0");
    else if (cmd == "QZ") Serial.println("0");

    // Initialization handshake
    else if (cmd == "INIT") {
      Serial.println("OK");
    }

    // Shutdown
    else if (cmd == "STOP") {
      Mouse.end();
    }
  }
}
```

### 2. Upload to Arduino

1. Open Arduino IDE
2. Select **Tools → Board → Arduino Leonardo** (or Pro Micro)
3. Select correct COM port under **Tools → Port**
4. Click **Upload** (Ctrl+U)

### 3. Verify Connection

```bash
# Windows: Check Device Manager
# Look for "Arduino Leonardo" under "Ports (COM & LPT)"
# Note the COM port number (e.g., COM3)
```

---

## Usage

### In Code

```cpp
#include "input/drivers/ArduinoDriver.h"

// Create driver with COM port
auto driver = std::make_unique<ArduinoDriver>("COM3", 115200);

// Initialize
if (!driver->initialize()) {
    LOG_ERROR("Failed to connect to Arduino on COM3");
    return;
}

// Use like any IMouseDriver
driver->move(10, -5);  // Move right 10px, up 5px
driver->click(MouseButton::Left);

// Cleanup
driver->shutdown();
```

### In InputManager Configuration

**Option 1: Hardcode in main.cpp**
```cpp
// Change from Win32Driver to ArduinoDriver
auto mouseDriver = std::make_unique<ArduinoDriver>("COM3", 115200);
```

**Option 2: Runtime Configuration (Future Phase 5)**
```json
// config/games/valorant.json
{
  "input": {
    "driver": "Arduino",
    "arduinoPort": "COM3",
    "arduinoBaudrate": 115200
  }
}
```

---

## Protocol Specification

### Commands (PC → Arduino)

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| **INIT** | `INIT\n` | Initialization handshake | `INIT\n` |
| **STOP** | `STOP\n` | Shutdown (disable mouse) | `STOP\n` |
| **Move** | `M,dx,dy\n` | Relative mouse movement | `M,10,-5\n` (right 10, up 5) |
| **Press** | `P{button}\n` | Press button down | `PL\n` (press left) |
| **Release** | `R{button}\n` | Release button | `RL\n` (release left) |
| **Click** | `C{button}\n` | Press + Release | `CL\n` (click left) |

**Button Codes:**
- `L` = Left
- `R` = Right
- `M` = Middle
- `S1` = Side 1 (forward)
- `S2` = Side 2 (back)

### Queries (PC → Arduino, blocking)

| Command | Format | Response | Description |
|---------|--------|----------|-------------|
| **QA** | `QA\n` | `0\n` or `1\n` | Is aim key pressed? |
| **QS** | `QS\n` | `0\n` or `1\n` | Is shoot key pressed? |
| **QZ** | `QZ\n` | `0\n` or `1\n` | Is zoom key pressed? |

**Note:** Hardware key monitoring requires physical buttons wired to Arduino pins (advanced feature, not implemented in basic firmware).

---

## Latency Comparison

| Method | Latency | Detection Risk | Notes |
|--------|---------|----------------|-------|
| **Win32Driver (SendInput)** | <1ms | High | Kernel API, easily detected |
| **ArduinoDriver (Serial HID)** | 2-5ms | Very Low | Hardware-level, appears as real USB device |

**Latency Breakdown (ArduinoDriver):**
- Serial transmission (115200 baud): ~1ms
- Arduino processing: <0.5ms
- USB HID poll interval: 1-2ms (configurable)
- **Total:** 2.5-3.5ms typical

---

## Troubleshooting

### "Failed to open COM3"
- **Check:** Arduino is connected and recognized in Device Manager
- **Fix:** Verify COM port number, try unplugging/replugging USB cable
- **Note:** Use Device Manager → Ports (COM & LPT) to find correct port

### "Write error: I/O error"
- **Check:** Serial connection lost
- **Fix:** Ensure USB cable is properly seated, try different USB port
- **Workaround:** ArduinoDriver will auto-reconnect on next `initialize()` call

### "libserial not found"
- **Check:** CMake can't find serial library
- **Fix:** Install via vcpkg or specify path manually:
  ```bash
  cmake -Dserial_DIR="C:/path/to/serial/lib/cmake/serial"
  ```

### Mouse movements not working
- **Check:** Arduino firmware uploaded correctly
- **Test:** Open Arduino Serial Monitor (115200 baud), type `M,10,10` manually
- **Expected:** Mouse should move diagonally

### Build fails with "serial/serial.h not found"
- **Check:** ENABLE_ARDUINO is ON but libserial not installed
- **Fix Option 1:** Install libserial (see Software Requirements)
- **Fix Option 2:** Disable Arduino support: `cmake -DENABLE_ARDUINO=OFF`

---

## Advanced: Hardware Key Monitoring

For hardware-based key detection (bypass software keyloggers):

### Wiring

```
Arduino Pin D2 → Aim Key Button → GND
Arduino Pin D3 → Shoot Key Button → GND
Arduino Pin D4 → Zoom Key Button → GND
```

### Updated Firmware

```cpp
// In setup():
pinMode(2, INPUT_PULLUP);  // Aim key
pinMode(3, INPUT_PULLUP);  // Shoot key
pinMode(4, INPUT_PULLUP);  // Zoom key

// In loop() queries section:
else if (cmd == "QA") Serial.println(digitalRead(2) == LOW ? "1" : "0");
else if (cmd == "QS") Serial.println(digitalRead(3) == LOW ? "1" : "0");
else if (cmd == "QZ") Serial.println(digitalRead(4) == LOW ? "1" : "0");
```

### Usage

```cpp
ArduinoDriver driver("COM3", 115200, true);  // Enable key monitoring

if (driver.isAimingActive()) {
    // Aim key pressed (hardware-level detection)
}
```

---

## Security Considerations

**Advantages:**
- ✅ Hardware-level mouse emulation (undetectable by kernel-mode anti-cheat)
- ✅ Appears as legitimate USB HID device in Device Manager
- ✅ No driver signature required

**Limitations:**
- ⚠️ Requires physical hardware ($5-10 Arduino board)
- ⚠️ Slightly higher latency than SendInput (2-5ms vs <1ms)
- ⚠️ USB device enumeration logged by OS (forensic trace)

**Detection Vectors:**
- Unusual mouse movement patterns (mitigated by humanization)
- Perfect consistency in timing (mitigated by timing variance)
- Statistical analysis of input behavior (mitigated by tremor/overshoot)

---

## Cost & Availability

**Arduino Pro Micro (ATmega32U4):**
- **Price:** $5-10 USD
- **Where to buy:** AliExpress, Amazon, SparkFun, Adafruit
- **Search terms:** "Arduino Pro Micro 5V 16MHz ATmega32U4"

**Alternative: Arduino Leonardo:**
- **Price:** $20-25 USD
- **Official Arduino board** (higher quality)

---

## References

- [Arduino Mouse Library](https://www.arduino.cc/reference/en/language/functions/usb/mouse/)
- [libserial Documentation](https://github.com/wjwwood/serial)
- [ATmega32U4 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7766-8-bit-AVR-ATmega16U4-32U4_Datasheet.pdf)
- [USB HID Specification](https://www.usb.org/hid)

---

**Last Updated:** 2025-12-30
**MacroMan AI Aimbot v2** - Arduino HID Driver
