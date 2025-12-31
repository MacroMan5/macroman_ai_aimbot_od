#pragma once

#include "SharedConfig.h"
#include <string>
#include <memory>
#include <windows.h>

namespace macroman {

/**
 * @brief Windows IPC manager for SharedConfig (memory-mapped file)
 *
 * Lifecycle:
 * 1. Engine creates mapping with createMapping()
 * 2. Config UI opens existing mapping with openMapping()
 * 3. Both processes access config via getConfig()
 * 4. Engine calls close() on shutdown (Config UI can crash safely)
 *
 * Thread Safety: SharedConfig uses lock-free atomics (see CRITICAL_TRAPS.md Trap #3)
 * Memory Safety: RAII - destructor automatically unmaps and closes handles
 *
 * Architecture Reference: Phase 5 - Configuration & Auto-Detection
 */
class SharedConfigManager {
public:
    SharedConfigManager();
    ~SharedConfigManager();

    // Disable copy (owns Windows handle)
    SharedConfigManager(const SharedConfigManager&) = delete;
    SharedConfigManager& operator=(const SharedConfigManager&) = delete;

    /**
     * @brief Create memory-mapped file (Engine side)
     *
     * @param mappingName Name of the shared memory object (default: "MacromanAimbot_Config")
     * @return true if creation succeeded, false otherwise
     *
     * Usage: Engine calls this once at startup before Config UI launches
     */
    bool createMapping(const std::string& mappingName = "MacromanAimbot_Config");

    /**
     * @brief Open existing memory-mapped file (Config UI side)
     *
     * @param mappingName Name of the shared memory object
     * @return true if opened successfully, false if Engine hasn't created it
     *
     * Usage: Config UI calls this to connect to Engine's shared memory
     */
    bool openMapping(const std::string& mappingName = "MacromanAimbot_Config");

    /**
     * @brief Close mapping and release resources
     *
     * Safe to call multiple times. Automatically called by destructor.
     */
    void close();

    /**
     * @brief Get pointer to shared config (both Engine and Config UI)
     *
     * @return Pointer to SharedConfig in shared memory, or nullptr if not mapped
     *
     * Thread Safety: Config fields are atomic - safe for concurrent access
     */
    SharedConfig* getConfig() const { return sharedConfig_; }

    /**
     * @brief Check if mapping is active
     */
    bool isActive() const { return sharedConfig_ != nullptr; }

    /**
     * @brief Get last error message
     */
    const std::string& getLastError() const { return lastError_; }

private:
    HANDLE hMapFile_;               // Handle to memory-mapped file
    SharedConfig* sharedConfig_;    // Pointer to mapped SharedConfig
    std::string mappingName_;
    std::string lastError_;
    bool isCreator_;                // True if this process created the mapping

    void setError(const std::string& error);
};

} // namespace macroman
