#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <string>
#include <vector>
#include "../Model.h"
#include "../scene/Scene.h"
#include "../anim/SkinnedModel.h"

class SceneRenderer {
public:
    GLuint program = 0;
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projMatrix{1.0f};
    glm::vec3 cameraPos{0.0f};

    bool init();
    void shutdown();

    void beginFrame(int fbw, int fbh, const glm::vec3& camPos, const glm::vec3& camTarget,
                    const SceneAtmosphere& atmosphere, const glm::vec3& lanternPos,
                    float will, float redGlow, float muzzleFlash);

    void drawBackdrops(const std::vector<SceneBackdropInstance>& backdrops);
    void drawGround(Model& ground);
    void drawSceneProps(const Scene& scene);
    void drawCreatures(const std::map<std::string, Model*>& creatureModels,
                       const std::vector<SceneCreatureInstance>& creatures, float time);
    void drawSkinnedCharacter(SkinnedModel& model, const glm::mat4& transform);
    void drawStaticModel(Model& model, const glm::mat4& transform);
};
