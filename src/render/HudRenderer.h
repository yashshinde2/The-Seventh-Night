#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "../game/GameMeters.h"

class HudRenderer {
public:
    HudRenderer() = default;
    ~HudRenderer();

    bool init();
    void shutdown();

    void begin(int screenWidth, int screenHeight);
    void drawRect(float x, float y, float width, float height, const glm::vec4& color);
    void drawBar(float x, float y, float width, float height, float value,
                 const glm::vec4& fillColor, const glm::vec4& bgColor, const glm::vec4& borderColor);
    void drawText(const std::string& text, float x, float y, float scale, const glm::vec4& color);
    void drawHUD(int screenWidth, int screenHeight, const PlayerMeters& meters,
                 int equippedWeapon, bool lanternOn, float time);
    void end();

private:
    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint fontTex_ = 0;

    int screenW_ = 1280;
    int screenH_ = 720;

    void initFontTexture();
};
