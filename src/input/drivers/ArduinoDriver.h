#pragma once

#include "../../core/interfaces/IMouseDriver.h"
#include "../../core/threading/ThreadSafeQueue.h"
#include <serial/serial.h>
#include <memory>
#include <mutex>
#include <string>

namespace sunone {

/**
 * @brief Arduino HID mouse driver implementation.
 *
 * Wraps the legacy SerialConnection class which communicates with
 * an Arduino running HID emulation firmware over serial port.
 */
class ArduinoDriver : public IMouseDriver {
public:
    ArduinoDriver(const std::string& port, int baudrate = 115200, bool enableKeys = false);
    ~ArduinoDriver() override;

    // Lifecycle
    bool initialize() override;
    void shutdown() override;

    // Core operations
    void move(int dx, int dy) override;
    void press(MouseButton button) override;
    void release(MouseButton button) override;
    void click(MouseButton button) override;

    // Info
    std::string getName() const override { return "Arduino HID"; }
    bool isConnected() const override;

    // Capabilities
    bool supportsHighPrecision() const override { return true; }

    // Hardware key monitoring (when enableKeys is true)
    bool isAimingActive() const;
    bool isShootingActive() const;
    bool isZoomingActive() const;
    bool supportsHardwareKeys() const;

private:
    void serialWorker();

    std::unique_ptr<serial::Serial> serial_;
    std::string port_;
    int baudrate_;
    bool enableKeys_;
    mutable std::mutex serialMutex_;

    // Async worker
    std::atomic<bool> running_{false};
    std::thread workerThread_;
    
    struct Command {
        std::string data;
    };
    ThreadSafeQueue<Command> queue_;
};

} // namespace sunone
