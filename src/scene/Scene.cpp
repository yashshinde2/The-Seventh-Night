#include "Scene.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string stripQuotes(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool Scene::loadFromFile(const std::string& scenePath, const fs::path& assetsRoot) {
    std::ifstream file(scenePath);
    if (!file.is_open()) {
        std::fprintf(stderr, "[Scene] Failed to open scene file: %s\n", scenePath.c_str());
        return false;
    }

    std::printf("[Scene] Loading scene from: %s\n", scenePath.c_str());

    std::string line;
    std::string currentSection = "";

    std::map<std::string, std::string> modelPaths;

    while (std::getline(file, line)) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;

        if (s.front() == '[' && s.back() == ']') {
            currentSection = s.substr(1, s.size() - 2);
            continue;
        }

        if (currentSection == "scene") {
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string key = trim(s.substr(0, eq));
                std::string val = trim(s.substr(eq + 1));
                std::stringstream ss(val);
                if (key == "name") {
                    atmosphere.name = stripQuotes(val);
                } else if (key == "ambient") {
                    ss >> atmosphere.ambient.x >> atmosphere.ambient.y >> atmosphere.ambient.z;
                } else if (key == "moon_dir") {
                    ss >> atmosphere.moonDir.x >> atmosphere.moonDir.y >> atmosphere.moonDir.z;
                } else if (key == "moon_color") {
                    ss >> atmosphere.moonColor.x >> atmosphere.moonColor.y >> atmosphere.moonColor.z;
                } else if (key == "fog_color") {
                    ss >> atmosphere.fogColor.x >> atmosphere.fogColor.y >> atmosphere.fogColor.z;
                } else if (key == "fog_density") {
                    ss >> atmosphere.fogDensity;
                } else if (key == "player_spawn") {
                    ss >> atmosphere.playerSpawn.x >> atmosphere.playerSpawn.y >> atmosphere.playerSpawn.z >> atmosphere.playerSpawnLane;
                }
            }
        } else if (currentSection == "backdrops") {
            // backdrop = texture_path x y z width height
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string val = trim(s.substr(eq + 1));
                std::stringstream ss(val);
                std::string texPath;
                float x = 0.0f, y = 13.0f, z = -22.0f, w = 85.0f, h = 38.0f;
                ss >> texPath >> x >> y >> z >> w >> h;
                texPath = stripQuotes(texPath);

                fs::path fullTexPath = assetsRoot / texPath;
                GLuint tex = Model::loadTextureFromFile(fullTexPath.string());
                Model quad = Model::backdropQuad(w, h, tex);

                SceneBackdropInstance b;
                b.texturePath = texPath;
                b.pos = glm::vec3(x, y, z);
                b.width = w;
                b.height = h;
                b.quadModel = std::move(quad);
                backdrops.push_back(std::move(b));
            }
        } else if (currentSection == "models") {
            // alias = relative_path
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string alias = trim(s.substr(0, eq));
                std::string relPath = stripQuotes(trim(s.substr(eq + 1)));
                modelPaths[alias] = relPath;
            }
        } else if (currentSection == "props") {
            // prop = alias pos_x pos_y pos_z scale rot_y lane
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string val = trim(s.substr(eq + 1));
                std::stringstream ss(val);
                std::string alias;
                float px = 0.0f, py = 0.0f, pz = 0.0f;
                float scale = 1.0f, rotY = 0.0f;
                int lane = 1;
                ss >> alias >> px >> py >> pz >> scale >> rotY >> lane;

                ScenePropInstance inst;
                inst.modelAlias = alias;
                inst.pos = glm::vec3(px, py, pz);
                inst.scale = scale;
                inst.rotY = rotY;
                inst.lane = lane;
                props.push_back(inst);
            }
        } else if (currentSection == "colliders") {
            // collider = min_x min_y min_z max_x max_y max_z type lane is_trigger
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string val = trim(s.substr(eq + 1));
                std::stringstream ss(val);
                float minX, minY, minZ, maxX, maxY, maxZ;
                std::string typeStr;
                int lane = 1;
                std::string trigStr;
                ss >> minX >> minY >> minZ >> maxX >> maxY >> maxZ >> typeStr >> lane >> trigStr;

                ObstaclePlatform plat;
                plat.bounds = AABB{glm::vec3(minX, minY, minZ), glm::vec3(maxX, maxY, maxZ)};
                plat.lane = lane;
                plat.isTrigger = (trigStr == "true" || trigStr == "1");

                if (typeStr == "OneWay") plat.type = SurfaceType::OneWay;
                else if (typeStr == "Ledge") plat.type = SurfaceType::Ledge;
                else if (typeStr == "ClimbableVine") plat.type = SurfaceType::ClimbableVine;
                else if (typeStr == "Slippery") plat.type = SurfaceType::Slippery;
                else plat.type = SurfaceType::Normal;

                platforms.push_back(plat);
            }
        } else if (currentSection == "creatures") {
            // creature = type pos_x pos_y pos_z
            size_t eq = s.find('=');
            if (eq != std::string::npos) {
                std::string val = trim(s.substr(eq + 1));
                std::stringstream ss(val);
                std::string cType;
                float cx = 0.0f, cy = 0.0f, cz = 0.0f;
                ss >> cType >> cx >> cy >> cz;

                SceneCreatureInstance c;
                c.type = cType;
                c.pos = glm::vec3(cx, cy, cz);
                creatures.push_back(c);
            }
        }
    }

    // Now load all model templates required by props
    std::printf("[Scene] Loading %zu distinct model templates defined in scene...\n", modelPaths.size());
    for (const auto& kv : modelPaths) {
        const std::string& alias = kv.first;
        const std::string& relPath = kv.second;
        fs::path fullPath = assetsRoot / relPath;

        Model m;
        bool ok = false;
        if (fullPath.extension() == ".obj") {
            ok = m.loadOBJ(fullPath.string());
        } else {
            ok = m.loadGLB(fullPath.string());
        }

        if (ok) {
            modelMap[alias] = modelTemplates.size();
            modelTemplates.push_back(std::move(m));
            std::printf("[Scene Asset] %-20s -> LOADED (%s)\n", alias.c_str(), relPath.c_str());
        } else {
            std::printf("[Scene Asset] %-20s -> FAILED (%s)\n", alias.c_str(), relPath.c_str());
        }
    }

    // Resolve modelIdx for each prop instance
    for (auto& p : props) {
        auto it = modelMap.find(p.modelAlias);
        if (it != modelMap.end()) {
            p.modelIdx = it->second;
        } else {
            p.modelIdx = (size_t)-1;
        }
    }

    printSummary();
    return true;
}

void Scene::printSummary() const {
    std::printf("\n================ Scene Summary ================\n");
    std::printf(" Scene Name:       %s\n", atmosphere.name.c_str());
    std::printf(" Models Loaded:    %zu\n", modelTemplates.size());
    std::printf(" Props Placed:     %zu\n", props.size());
    std::printf(" Platforms/Boxes:  %zu\n", platforms.size());
    std::printf(" Backdrops:        %zu\n", backdrops.size());
    std::printf(" Creatures Spawn:  %zu\n", creatures.size());
    std::printf(" Player Spawn:     (%.1f, %.1f, %.1f) Lane: %d\n",
                atmosphere.playerSpawn.x, atmosphere.playerSpawn.y, atmosphere.playerSpawn.z, atmosphere.playerSpawnLane);
    std::printf("===============================================\n\n");
    std::fflush(stdout);
}

bool Scene::validate() const {
    if (modelTemplates.empty()) {
        std::fprintf(stderr, "[Scene Error] No models loaded!\n");
        return false;
    }
    if (props.empty()) {
        std::fprintf(stderr, "[Scene Error] No props placed in scene!\n");
        return false;
    }
    if (platforms.empty()) {
        std::fprintf(stderr, "[Scene Error] No platforms/colliders defined in scene!\n");
        return false;
    }
    return true;
}
