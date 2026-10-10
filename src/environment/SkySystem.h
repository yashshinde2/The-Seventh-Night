#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

class SkySystem {
public:
    SkySystem() = default;
    ~SkySystem();

    void init();
    void update(float dt);
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos);

private:
    void initSkyDome();
    void initStars();
    void initMoon();
    void initMountains();

    GLuint skyShader_ = 0;
    GLuint mountainShader_ = 0;
    GLuint moonShader_ = 0;
    GLuint starShader_ = 0;

    // Sky Dome Mesh
    GLuint skyVao_ = 0, skyVbo_ = 0;
    GLsizei skyVertexCount_ = 0;

    // Stars
    GLuint starsVao_ = 0, starsVbo_ = 0;
    GLsizei starCount_ = 0;

    // Moon
    GLuint moonVao_ = 0, moonVbo_ = 0;
    GLsizei moonVertexCount_ = 0;

    // Far Mountain Ridge
    GLuint farRidgeVao_ = 0, farRidgeVbo_ = 0;
    GLsizei farRidgeCount_ = 0;

    // Mid Mountain Ridge
    GLuint midRidgeVao_ = 0, midRidgeVbo_ = 0;
    GLsizei midRidgeCount_ = 0;

    float time_ = 0.0f;
    float cloudScroll_ = 0.0f;
};
