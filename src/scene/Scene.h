#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include "../Model.h"
#include "../physics/PhysicsEngine.h"

namespace fs = std::filesystem;

struct SceneAtmosphere {
    std::string name = "Kaalvan Forest";
    glm::vec3 ambient{0.32f, 0.38f, 0.46f};
    glm::vec3 moonDir{-0.4f, 1.0f, 0.6f};
    glm::vec3 moonColor{0.42f, 0.54f, 0.72f};
    glm::vec3 fogColor{0.08f, 0.12f, 0.18f};
    float fogDensity = 0.018f;
    glm::vec3 playerSpawn{0.0f, 0.0f, 0.0f};
    int playerSpawnLane = 1;
};

struct ScenePropInstance {
    size_t modelIdx = 0;
    std::string modelAlias;
    glm::vec3 pos{0.0f};
    float scale = 1.0f;
    float rotY = 0.0f;
    int lane = 1; // 0 = FG, 1 = MID, 2 = BG
};

struct SceneBackdropInstance {
    std::string texturePath;
    glm::vec3 pos{0.0f};
    float width = 85.0f;
    float height = 38.0f;
    Model quadModel;
};

struct SceneCreatureInstance {
    std::string type;
    glm::vec3 pos{0.0f};
};

class Scene {
public:
    SceneAtmosphere atmosphere;
    std::vector<Model> modelTemplates;
    std::map<std::string, size_t> modelMap;
    std::vector<ScenePropInstance> props;
    std::vector<ObstaclePlatform> platforms;
    std::vector<SceneBackdropInstance> backdrops;
    std::vector<SceneCreatureInstance> creatures;

    bool loadFromFile(const std::string& scenePath, const fs::path& assetsRoot);
    void printSummary() const;
    bool validate() const;
};
