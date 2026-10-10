#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class AssetManager {
public:
    static fs::path findAssetsRoot(const char* argv0 = nullptr);
    static void printDiscoveredAssets(const fs::path& assetsRoot);
};
