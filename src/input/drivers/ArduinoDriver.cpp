#include "ArduinoDriver.h"
#include <iostream>
#include <thread>

namespace macroman {

ArduinoDriver::ArduinoDriver(const std::string& port, int baudrate, bool enableKeys)
    : port_(port)
    , baudrate_(baudrate)
    , enableKeys_(enableKeys)
{
    // Start the worker thread
    running_ = true;
    workerThread_ = std::thread(&ArduinoDriver::serialWorker, this);
}

ArduinoDriver::~ArduinoDriver() {
    shutdown();
}

bool ArduinoDriver::initialize() {
    try {
        std::lock_guard<std::mutex> lock(serialMutex_);
        serial_ = std::make_unique<serial::Serial>(
            port_,
            baudrate_,
            serial::Timeout::simpleTimeout(1000)
        );

        if (!serial_->isOpen()) {
            std::cerr << "[ArduinoDriver] Failed to open " << port_ << std::endl;
            return false;
        }

        // Send initialization handshake directly (bypass queue for init)
        serial_->write("INIT\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::cout << "[ArduinoDriver] Connected to " << port_
                  << " at " << baudrate_ << " baud" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ArduinoDriver] Initialize error: " << e.what() << std::endl;
        return false;
    }
}

void ArduinoDriver::shutdown() {
    running_ = false;
    // Wake up worker if it's waiting (dummy push if needed, or check queue implementation)
    // Here we rely on the queue timeout or polling in worker
    
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    std::lock_guard<std::mutex> lock(serialMutex_);
    if (serial_ && serial_->isOpen()) {
        try {
            serial_->write("STOP\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            serial_->close();
        } catch (const std::exception& e) {
            std::cerr << "[ArduinoDriver] Shutdown error: " << e.what() << std::endl;
        }
    }

    serial_.reset();
}

void ArduinoDriver::serialWorker() {
    while (running_) {
        // Wait for command with timeout to check running_ flag periodically
        auto cmdOpt = queue_.waitPop(std::chrono::milliseconds(100));
        
        if (cmdOpt) {
            std::lock_guard<std::mutex> lock(serialMutex_);
            if (serial_ && serial_->isOpen()) {
                try {
                    serial_->write(cmdOpt->data);
                    // serial_->flush(); // Avoid flush if possible for lower latency
                } catch (const std::exception& e) {
                    std::cerr << "[ArduinoDriver] Write error: " << e.what() << std::endl;
                }
            }
        }
    }
}

void ArduinoDriver::move(int dx, int dy) {
    if (!isConnected()) return;
    
    // Protocol: "M,dx,dy\n"
    std::string cmd = "M," + std::to_string(dx) + "," + std::to_string(dy) + "\n";
    queue_.push({cmd});
}

void ArduinoDriver::press(MouseButton button) {
    if (!isConnected()) return;

    std::string cmd;
    switch (button) {
        case MouseButton::Left:   cmd = "PL\n"; break;
        case MouseButton::Right:  cmd = "PR\n"; break;
        case MouseButton::Middle: cmd = "PM\n"; break;
        case MouseButton::Side1:  cmd = "PS1\n"; break;
        case MouseButton::Side2:  cmd = "PS2\n"; break;
        default: return;
    }
    queue_.push({cmd});
}

void ArduinoDriver::release(MouseButton button) {
    if (!isConnected()) return;

    std::string cmd;
    switch (button) {
        case MouseButton::Left:   cmd = "RL\n"; break;
        case MouseButton::Right:  cmd = "RR\n"; break;
        case MouseButton::Middle: cmd = "RM\n"; break;
        case MouseButton::Side1:  cmd = "RS1\n"; break;
        case MouseButton::Side2:  cmd = "RS2\n"; break;
        default: return;
    }
    queue_.push({cmd});
}

void ArduinoDriver::click(MouseButton button) {
    if (!isConnected()) return;

    std::string cmd;
    switch (button) {
        case MouseButton::Left:   cmd = "CL\n"; break;
        case MouseButton::Right:  cmd = "CR\n"; break;
        case MouseButton::Middle: cmd = "CM\n"; break;
        case MouseButton::Side1:  cmd = "CS1\n"; break;
        case MouseButton::Side2:  cmd = "CS2\n"; break;
        default: return;
    }
    queue_.push({cmd});
}

bool ArduinoDriver::isConnected() const {
    std::lock_guard<std::mutex> lock(serialMutex_);
    return serial_ && serial_->isOpen();
}

// Queries are blocking/synchronous by nature as they need a return value
// We bypass the queue for these to get immediate response
bool ArduinoDriver::isAimingActive() const {
    if (!enableKeys_ || !isConnected()) return false;

    std::lock_guard<std::mutex> lock(serialMutex_);

    try {
        serial_->write("QA\n");
        // serial_->flush();

        std::string response = serial_->readline(100, "\n");
        return response.find("1") != std::string::npos;
    } catch (const std::exception&) {
        return false;
    }
}

bool ArduinoDriver::isShootingActive() const {
    if (!enableKeys_ || !isConnected()) return false;

    std::lock_guard<std::mutex> lock(serialMutex_);

    try {
        serial_->write("QS\n");
        
        std::string response = serial_->readline(100, "\n");
        return response.find("1") != std::string::npos;
    } catch (const std::exception&) {
        return false;
    }
}

bool ArduinoDriver::isZoomingActive() const {
    if (!enableKeys_ || !isConnected()) return false;

    std::lock_guard<std::mutex> lock(serialMutex_);

    try {
        serial_->write("QZ\n");

        std::string response = serial_->readline(100, "\n");
        return response.find("1") != std::string::npos;
    } catch (const std::exception&) {
        return false;
    }
}

bool ArduinoDriver::supportsHardwareKeys() const {
    return enableKeys_;
}

} // namespace macroman