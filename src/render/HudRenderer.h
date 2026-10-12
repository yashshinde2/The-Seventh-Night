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
                 int equippedWeapon, bool lanternOn, float time,
                 const std::string& dayTitle = "ACT I - DAY 1: KAALVAN FOREST",
                 const std::string& charTitle = "AARAV");
    void drawDialogueBox(int screenWidth, int screenHeight, const std::string& speaker,
                         const std::string& line, const glm::vec4& speakerColor = glm::vec4(1.0f, 0.78f, 0.35f, 1.0f));
    void drawChoiceBox(int screenWidth, int screenHeight, const std::string& opt1Key,
                       const std::string& opt1Text, const std::string& opt2Key, const std::string& opt2Text);
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
