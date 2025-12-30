#include "Win32Driver.h"

namespace macroman {

bool Win32Driver::initialize() {
    // Win32 SendInput is always available on Windows
    return true;
}

void Win32Driver::shutdown() {
    // No cleanup needed for Win32 API
}

void Win32Driver::move(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }

    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void Win32Driver::press(MouseButton button) {
    DWORD flags = buttonToDownFlag(button);
    if (flags == 0) {
        return;
    }

    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
}

void Win32Driver::release(MouseButton button) {
    DWORD flags = buttonToUpFlag(button);
    if (flags == 0) {
        return;
    }

    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
}

void Win32Driver::click(MouseButton button) {
    press(button);
    release(button);
}

DWORD Win32Driver::buttonToDownFlag(MouseButton button) const {
    switch (button) {
        case MouseButton::Left:
            return MOUSEEVENTF_LEFTDOWN;
        case MouseButton::Right:
            return MOUSEEVENTF_RIGHTDOWN;
        case MouseButton::Middle:
            return MOUSEEVENTF_MIDDLEDOWN;
        case MouseButton::Side1:
            return MOUSEEVENTF_XDOWN;
        case MouseButton::Side2:
            return MOUSEEVENTF_XDOWN;
        default:
            return 0;
    }
}

DWORD Win32Driver::buttonToUpFlag(MouseButton button) const {
    switch (button) {
        case MouseButton::Left:
            return MOUSEEVENTF_LEFTUP;
        case MouseButton::Right:
            return MOUSEEVENTF_RIGHTUP;
        case MouseButton::Middle:
            return MOUSEEVENTF_MIDDLEUP;
        case MouseButton::Side1:
            return MOUSEEVENTF_XUP;
        case MouseButton::Side2:
            return MOUSEEVENTF_XUP;
        default:
            return 0;
    }
}

} // namespace macroman
