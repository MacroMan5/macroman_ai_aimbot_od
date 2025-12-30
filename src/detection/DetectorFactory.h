#pragma once

#include "../../core/interfaces/IDetector.h"
#include "../../core/types/Enums.h"
#include <memory>
#include <vector>

namespace macroman {

/**
 * Factory for creating detector instances based on backend type.
 * Provides runtime backend selection and availability checking.
 */
class DetectorFactory {
public:
    /**
     * Create a detector of the specified type.
     * @param type The detector backend type to create
     * @return Unique pointer to the created detector, or nullptr on failure
     * @throws std::runtime_error if the backend is not available
     */
    static std::unique_ptr<IDetector> create(DetectorType type);

    /**
     * Auto-detect and return the best available backend.
     * Prefers TensorRT if CUDA is available, otherwise DirectML.
     * @return Best available detector type
     */
    static DetectorType getBestAvailable();

    /**
     * Check if a specific backend is available.
     * @param type The backend type to check
     * @return true if the backend can be used
     */
    static bool isAvailable(DetectorType type);

    /**
     * Get a list of all available backends.
     * @return Vector of available detector types
     */
    static std::vector<DetectorType> getAvailableBackends();

    /**
     * Get the display name for a detector type.
     * @param type The detector type
     * @return Human-readable name string
     */
    static const char* getTypeName(DetectorType type);
};

} // namespace macroman
