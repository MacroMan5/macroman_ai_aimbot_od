#include "SharedConfigManager.h"
#include "utils/Logger.h"
#include <sstream>

namespace macroman {

SharedConfigManager::SharedConfigManager()
    : hMapFile_(nullptr)
    , sharedConfig_(nullptr)
    , isCreator_(false)
{
}

SharedConfigManager::~SharedConfigManager() {
    close();
}

bool SharedConfigManager::createMapping(const std::string& mappingName) {
    if (isActive()) {
        setError("Mapping already active - call close() first");
        return false;
    }

    mappingName_ = mappingName;

    // Create memory-mapped file (Engine side)
    hMapFile_ = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // Use paging file (not disk file)
        nullptr,                 // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // High-order DWORD of size (0 for small objects)
        sizeof(SharedConfig),    // Low-order DWORD of size
        mappingName_.c_str()     // Name of mapping object
    );

    if (hMapFile_ == nullptr) {
        DWORD error = GetLastError();
        std::ostringstream oss;
        oss << "CreateFileMapping failed (error " << error << ")";
        setError(oss.str());
        LOG_ERROR("SharedConfigManager: {}", lastError_);
        return false;
    }

    // Check if mapping already existed (another Engine instance running?)
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
        setError("Mapping already exists - another Engine instance running?");
        LOG_ERROR("SharedConfigManager: {}", lastError_);
        return false;
    }

    // Map view of file into process address space
    sharedConfig_ = static_cast<SharedConfig*>(
        MapViewOfFile(
            hMapFile_,           // Handle to map object
            FILE_MAP_ALL_ACCESS, // Read/write permission
            0,                   // High-order DWORD of offset
            0,                   // Low-order DWORD of offset
            sizeof(SharedConfig) // Number of bytes to map
        )
    );

    if (sharedConfig_ == nullptr) {
        DWORD error = GetLastError();
        std::ostringstream oss;
        oss << "MapViewOfFile failed (error " << error << ")";
        setError(oss.str());
        LOG_ERROR("SharedConfigManager: {}", lastError_);
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
        return false;
    }

    // Initialize SharedConfig to default values (Engine is creator)
    sharedConfig_->reset();
    isCreator_ = true;

    LOG_INFO("SharedConfigManager: Created mapping '{}' ({} bytes)",
             mappingName_, sizeof(SharedConfig));
    return true;
}

bool SharedConfigManager::openMapping(const std::string& mappingName) {
    if (isActive()) {
        setError("Mapping already active - call close() first");
        return false;
    }

    mappingName_ = mappingName;

    // Open existing memory-mapped file (Config UI side)
    hMapFile_ = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,  // Read/write access
        FALSE,                // Don't inherit handle
        mappingName_.c_str()  // Name of mapping object
    );

    if (hMapFile_ == nullptr) {
        DWORD error = GetLastError();
        std::ostringstream oss;
        oss << "OpenFileMapping failed (error " << error << ") - is Engine running?";
        setError(oss.str());
        LOG_ERROR("SharedConfigManager: {}", lastError_);
        return false;
    }

    // Map view of file into process address space
    sharedConfig_ = static_cast<SharedConfig*>(
        MapViewOfFile(
            hMapFile_,           // Handle to map object
            FILE_MAP_ALL_ACCESS, // Read/write permission
            0,                   // High-order DWORD of offset
            0,                   // Low-order DWORD of offset
            sizeof(SharedConfig) // Number of bytes to map
        )
    );

    if (sharedConfig_ == nullptr) {
        DWORD error = GetLastError();
        std::ostringstream oss;
        oss << "MapViewOfFile failed (error " << error << ")";
        setError(oss.str());
        LOG_ERROR("SharedConfigManager: {}", lastError_);
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
        return false;
    }

    isCreator_ = false;
    LOG_INFO("SharedConfigManager: Opened mapping '{}' ({} bytes)",
             mappingName_, sizeof(SharedConfig));
    return true;
}

void SharedConfigManager::close() {
    if (sharedConfig_) {
        UnmapViewOfFile(sharedConfig_);
        sharedConfig_ = nullptr;
    }

    if (hMapFile_) {
        CloseHandle(hMapFile_);
        hMapFile_ = nullptr;
    }

    if (!mappingName_.empty()) {
        LOG_INFO("SharedConfigManager: Closed mapping '{}'", mappingName_);
        mappingName_.clear();
    }

    isCreator_ = false;
}

void SharedConfigManager::setError(const std::string& error) {
    lastError_ = error;
}

} // namespace macroman
