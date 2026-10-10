#include "Assets.h"

#include <iostream>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

fs::path AssetManager::findAssetsRoot(const char* argv0) {
    std::vector<fs::path> searchBases;

    // Base 1: Current working directory
    try {
        searchBases.push_back(fs::current_path());
    } catch (...) {}

    // Base 2: Executable directory (Windows API or argv[0])
#ifdef _WIN32
    char exePathBuf[1024] = {0};
    if (GetModuleFileNameA(NULL, exePathBuf, sizeof(exePathBuf)) > 0) {
        fs::path exePath(exePathBuf);
        searchBases.push_back(exePath.parent_path());
    }
#endif

    if (argv0 && std::strlen(argv0) > 0) {
        fs::path argPath(argv0);
        searchBases.push_back(argPath.parent_path());
    }

    // Relative candidates to check
    std::vector<fs::path> relCandidates = {
        "assets",
        "../assets",
        "../../assets",
        "../../../assets"
    };

    for (const auto& base : searchBases) {
        for (const auto& rel : relCandidates) {
            fs::path candidate = fs::weakly_canonical(base / rel);
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                std::printf("[AssetManager] Resolved assets root: %s\n", candidate.string().c_str());
                std::fflush(stdout);
                return candidate;
            }
        }
    }

    std::fprintf(stderr, "\n[ERROR] AssetManager: COULD NOT FIND 'assets' DIRECTORY!\n");
    std::fprintf(stderr, "Checked paths relative to CWD (%s) and Executable location.\n\n",
                 fs::current_path().string().c_str());
    std::fflush(stderr);
    return fs::path();
}

void AssetManager::printDiscoveredAssets(const fs::path& assetsRoot) {
    if (assetsRoot.empty() || !fs::exists(assetsRoot)) {
        std::printf("[AssetManager] No assets directory to scan.\n");
        return;
    }

    std::printf("--- Discovered Assets in %s ---\n", assetsRoot.string().c_str());
    size_t count = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(assetsRoot)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    std::printf(" [%zu] %s (%.2f MB)\n", ++count,
                                fs::relative(entry.path(), assetsRoot).string().c_str(),
                                (double)entry.file_size() / (1024.0 * 1024.0));
                }
            }
        }
    } catch (const std::exception& e) {
        std::printf("Error scanning assets: %s\n", e.what());
    }
    std::printf("Total glTF/GLB models found: %zu\n-----------------------------------\n", count);
    std::fflush(stdout);
}
