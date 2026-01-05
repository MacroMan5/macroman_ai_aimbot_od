#include "AppConfig.h"
#include <iostream>

namespace macroman {

void ConfigManager::load(const std::string& path) {
    std::cout << "[ConfigManager] Loading config from " << path << " (Not implemented)" << std::endl;
}

void ConfigManager::save(const std::string& path) {
    std::cout << "[ConfigManager] Saving config to " << path << " (Not implemented)" << std::endl;
}

}
