#pragma once
#include <string>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace macroman {

/**
 * Path utility functions for normalizing and validating file paths
 */
class PathUtils {
public:
    /**
     * Normalize a file path:
     * - Replace backslashes with forward slashes
     * - Remove leading slashes for relative paths (e.g., "/models/" → "models/")
     * - Resolve relative paths to absolute paths
     *
     * @param path The path to normalize
     * @return Normalized path string
     */
    static std::string normalize(const std::string& path) {
        if (path.empty()) {
            return path;
        }

        std::string normalized = path;

        // Replace all backslashes with forward slashes
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        // Remove double slashes that might have been created by config concatenation
        // e.g. "//models" -> "/models"
        size_t pos;
        while ((pos = normalized.find("//")) != std::string::npos) {
            normalized.replace(pos, 2, "/");
        }

        // Remove leading slash if it looks like a relative path that got an extra slash
        if (normalized.size() >= 1 && normalized[0] == '/' && 
            (normalized.size() == 1 || normalized[1] != '/')) {
            
            // Heuristic: if the path starting with / doesn't exist, try removing the /
            if (!std::filesystem::exists(normalized)) {
                 if (normalized.size() > 1) {
                     std::string withoutSlash = normalized.substr(1);
                     if (std::filesystem::exists(withoutSlash) || 
                         std::filesystem::exists(resolvePath(withoutSlash))) {
                         normalized = withoutSlash;
                     }
                 }
            }
        }

        // Try to resolve the path if it doesn't exist as-is
        if (!std::filesystem::exists(normalized)) {
            std::string resolved = resolvePath(normalized);
            if (!resolved.empty()) {
                normalized = resolved;
            }
        }

        // Make absolute
        try {
            std::filesystem::path fsPath(normalized);
            if (!fsPath.is_absolute()) {
                fsPath = std::filesystem::absolute(fsPath);
                normalized = fsPath.string();
            }
        } catch (...) {
            // Ignore
        }
        
        // Final slash cleanup after absolute conversion
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        return normalized;
    }

    /**
     * Search for a file in common relative locations
     */
    static std::string resolvePath(const std::string& path) {
        std::filesystem::path p(path);
        if (p.is_absolute() && std::filesystem::exists(p)) {
            return p.string();
        }

        // Search locations relative to current working directory (which is usually x64/Release/...)
        std::vector<std::string> searchBases = {
            ".",
            "..",
            "../..",           // Common for build dirs (x64/DML -> root)
            "../../..",
            "src",
            "../src"
        };

        for (const auto& base : searchBases) {
            try {
                std::filesystem::path tryPath = std::filesystem::path(base) / p;
                if (std::filesystem::exists(tryPath) && std::filesystem::is_regular_file(tryPath)) {
                    return std::filesystem::absolute(tryPath).string();
                }
            } catch (...) {}
        }

        return ""; // Not found
    }

    /**
     * Check if a model file exists and is readable
     *
     * @param path The path to check
     * @return true if file exists and is a regular file, false otherwise
     */
    static bool isValidModelFile(const std::string& path) {
        try {
            // Don't use normalize here to avoid infinite recursion if normalize calls us
            // But we can use resolvePath
            if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) return true;
            
            std::string resolved = resolvePath(path);
            return !resolved.empty();
        } catch (...) {
            return false;
        }
    }
};

} // namespace macroman

