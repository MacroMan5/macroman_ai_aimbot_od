#pragma once

namespace macroman {

enum class CaptureMethod {
    DesktopDuplication,
    WinRT,
    VirtualCamera,
    Mock // For testing
};

enum class DetectorType {
    TensorRT,
    DirectML,
    Mock // For testing
};

enum class InputMethod {
    Win32,
    GHub,
    Arduino,
    KmboxB,
    KmboxNet
};

enum class MouseButton {
    Left,
    Right,
    Middle,
    Side1,
    Side2
};

enum class TargetPivot {
    Center,
    Top,        // Head
    TopThird,
    Custom
};

} // namespace macroman
