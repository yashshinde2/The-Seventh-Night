#include "HudRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cmath>
#include <vector>

// Minimal 8x8 bitmap font data for standard printable ASCII (characters 32 to 126)
// Standard compact 5x7 / 8x8 font matrix representation
static const uint8_t kFontData[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // '#'
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // '$'
    {0x00,0x63,0x66,0x0C,0x18,0x33,0x63,0x00}, // '%'
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, // '&'
    {0x30,0x30,0x10,0x20,0x00,0x00,0x00,0x00}, // '''
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // '('
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // '.'
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, // '/'
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // '0'
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // '1'
    {0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00}, // '2'
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // '3'
    {0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00}, // '4'
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // '5'
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // '6'
    {0x7E,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00}, // '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // '8'
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // '9'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // ':'
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // ';'
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '<'
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // '='
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}, // '>'
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00}, // '?'
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // '@'
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // 'A'
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 'B'
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // 'C'
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 'D'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // 'E'
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // 'F'
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00}, // 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'I'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // 'J'
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 'K'
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 'L'
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 'M'
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // 'N'
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'O'
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // 'P'
    {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}, // 'Q'
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, // 'R'
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // 'S'
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'V'
    {0x63,0x63,0x6B,0x7F,0x77,0x63,0x63,0x00}, // 'W'
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // 'Y'
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // 'Z'
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // '['
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, // '\'
    {0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00}, // '_'
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // 'a'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 'b'
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // 'c'
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // 'd'
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 'e'
    {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, // 'f'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // 'g'
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 'h'
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 'i'
    {0x0C,0x00,0x0C,0x0C,0x0C,0x0C,0x6C,0x38}, // 'j'
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // 'k'
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'l'
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 'n'
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 'o'
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 'p'
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x07}, // 'q'
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 'r'
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 's'
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0C,0x00}, // 't'
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x36,0x36,0x00}, // 'w'
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // 'x'
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // 'y'
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 'z'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // '}'
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  // '~'
};

static const char* kHudVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

uniform mat4 uOrtho;

out vec2 vUV;
out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = uOrtho * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kHudFragmentShader = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;

uniform sampler2D uFontTex;
uniform int uIsText;

out vec4 frag;

void main() {
    if (uIsText == 1) {
        float alpha = texture(uFontTex, vUV).r;
        if (alpha < 0.1) discard;
        frag = vec4(vColor.rgb, vColor.a * alpha);
    } else {
        frag = vColor;
    }
}
)";

struct Vertex2D {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 color;
};

HudRenderer::~HudRenderer() {
    shutdown();
}

void HudRenderer::initFontTexture() {
    // 16 chars per row, 6 rows (96 glyphs), each 8x8 -> 128x64 atlas (padded to 128x128)
    const int atlasW = 128;
    const int atlasH = 128;
    std::vector<uint8_t> pixels(atlasW * atlasH, 0);

    for (int i = 0; i < 95; ++i) {
        int charCol = i % 16;
        int charRow = i / 16;
        int startX = charCol * 8;
        int startY = charRow * 8;

        for (int y = 0; y < 8; ++y) {
            uint8_t rowByte = kFontData[i][y];
            for (int x = 0; x < 8; ++x) {
                if (rowByte & (0x80 >> x)) {
                    pixels[(startY + y) * atlasW + (startX + x)] = 255;
                }
            }
        }
    }

    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool HudRenderer::init() {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &kHudVertexShader, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &kHudFragmentShader, nullptr);
    glCompileShader(fs);

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(Vertex2D), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, pos));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, uv));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (void*)offsetof(Vertex2D, color));

    glBindVertexArray(0);

    initFontTexture();
    std::printf("[HUD] HudRenderer initialized with procedural 8x8 font atlas\n");
    return true;
}

void HudRenderer::shutdown() {
    if (fontTex_) { glDeleteTextures(1, &fontTex_); fontTex_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }
}

void HudRenderer::begin(int screenWidth, int screenHeight) {
    screenW_ = screenWidth > 0 ? screenWidth : 1280;
    screenH_ = screenHeight > 0 ? screenHeight : 720;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(program_);
    glm::mat4 ortho = glm::ortho(0.0f, (float)screenW_, (float)screenH_, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glUniform1i(glGetUniformLocation(program_, "uFontTex"), 0);

    glBindVertexArray(vao_);
}

void HudRenderer::end() {
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void HudRenderer::drawRect(float x, float y, float width, float height, const glm::vec4& color) {
    glUniform1i(glGetUniformLocation(program_, "uIsText"), 0);

    Vertex2D quad[6] = {
        {{x, y},                 {0.0f, 0.0f}, color},
        {{x + width, y},         {1.0f, 0.0f}, color},
        {{x + width, y + height},{1.0f, 1.0f}, color},

        {{x, y},                 {0.0f, 0.0f}, color},
        {{x + width, y + height},{1.0f, 1.0f}, color},
        {{x, y + height},        {0.0f, 1.0f}, color}
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void HudRenderer::drawBar(float x, float y, float width, float height, float value,
                         const glm::vec4& fillColor, const glm::vec4& bgColor, const glm::vec4& borderColor) {
    // Border frame
    drawRect(x - 2.0f, y - 2.0f, width + 4.0f, height + 4.0f, borderColor);
    // Dark background trough
    drawRect(x, y, width, height, bgColor);
    // Filled bar
    float clampedVal = glm::clamp(value, 0.0f, 1.0f);
    if (clampedVal > 0.005f) {
        drawRect(x, y, width * clampedVal, height, fillColor);
    }
}

void HudRenderer::drawText(const std::string& text, float x, float y, float scale, const glm::vec4& color) {
    glUniform1i(glGetUniformLocation(program_, "uIsText"), 1);

    std::vector<Vertex2D> vertices;
    vertices.reserve(text.size() * 6);

    float curX = x;
    const float glyphW = 8.0f * scale;
    const float glyphH = 8.0f * scale;

    for (char c : text) {
        if (c < 32 || c > 126) c = ' ';
        int idx = c - 32;
        int col = idx % 16;
        int row = idx / 16;

        float u0 = (col * 8.0f) / 128.0f;
        float v0 = (row * 8.0f) / 128.0f;
        float u1 = u0 + (8.0f / 128.0f);
        float v1 = v0 + (8.0f / 128.0f);

        vertices.push_back({{curX, y},                 {u0, v0}, color});
        vertices.push_back({{curX + glyphW, y},         {u1, v0}, color});
        vertices.push_back({{curX + glyphW, y + glyphH},{u1, v1}, color});

        vertices.push_back({{curX, y},                 {u0, v0}, color});
        vertices.push_back({{curX + glyphW, y + glyphH},{u1, v1}, color});
        vertices.push_back({{curX, y + glyphH},        {u0, v1}, color});

        curX += glyphW;
    }

    if (!vertices.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex2D), vertices.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    }
}

void HudRenderer::drawHUD(int screenWidth, int screenHeight, const PlayerMeters& meters,
                         int equippedWeapon, bool lanternOn, float time) {
    begin(screenWidth, screenHeight);

    // --- 1. Top-Left Survival Horror Meters Panel (Glassmorphic dark container) ---
    float panelX = 24.0f;
    float panelY = 24.0f;
    float panelW = 280.0f;
    float panelH = 135.0f;

    // Panel backdrop & subtle border
    drawRect(panelX, panelY, panelW, panelH, glm::vec4(0.04f, 0.06f, 0.09f, 0.82f));
    drawRect(panelX - 1.0f, panelY - 1.0f, panelW + 2.0f, 1.0f, glm::vec4(0.25f, 0.35f, 0.45f, 0.6f));
    drawRect(panelX - 1.0f, panelY + panelH, panelW + 2.0f, 1.0f, glm::vec4(0.25f, 0.35f, 0.45f, 0.6f));

    // Meter Bar Dimensions
    float barX = panelX + 75.0f;
    float barW = 185.0f;
    float barH = 12.0f;

    glm::vec4 bgBarCol(0.10f, 0.12f, 0.15f, 0.9f);
    glm::vec4 borderCol(0.20f, 0.25f, 0.32f, 0.8f);

    // Meter 1: WILL (Lantern Gold / Sanity) - Pulses red if low
    float willY = panelY + 16.0f;
    glm::vec4 willCol(0.95f, 0.72f, 0.24f, 1.0f);
    if (meters.will < 0.25f) {
        float pulse = 0.6f + 0.4f * std::sin(time * 8.0f);
        willCol = glm::mix(glm::vec4(0.95f, 0.2f, 0.1f, 1.0f), glm::vec4(0.95f, 0.72f, 0.24f, 1.0f), pulse);
    }
    drawText("WILL", panelX + 12.0f, willY + 1.0f, 1.25f, glm::vec4(0.9f, 0.9f, 0.9f, 0.95f));
    drawBar(barX, willY, barW, barH, meters.will, willCol, bgBarCol, borderCol);

    // Meter 2: BODY (Health / Crimson)
    float bodyY = willY + 26.0f;
    glm::vec4 bodyCol(0.85f, 0.22f, 0.20f, 1.0f);
    drawText("BODY", panelX + 12.0f, bodyY + 1.0f, 1.25f, glm::vec4(0.9f, 0.9f, 0.9f, 0.95f));
    drawBar(barX, bodyY, barW, barH, meters.body, bodyCol, bgBarCol, borderCol);

    // Meter 3: STAMINA (Energy / Emerald)
    float stamY = bodyY + 26.0f;
    glm::vec4 stamCol = meters.isExhausted ? glm::vec4(0.7f, 0.4f, 0.2f, 1.0f) : glm::vec4(0.22f, 0.75f, 0.42f, 1.0f);
    drawText("STAMINA", panelX + 12.0f, stamY + 1.0f, 1.15f, glm::vec4(0.9f, 0.9f, 0.9f, 0.95f));
    drawBar(barX, stamY, barW, barH, meters.stamina, stamCol, bgBarCol, borderCol);

    // Meter 4: HOPE (Ending Determinant / Soul Blue Luminescence)
    float hopeY = stamY + 26.0f;
    glm::vec4 hopeCol(0.35f, 0.65f, 0.95f, 1.0f);
    drawText("HOPE", panelX + 12.0f, hopeY + 1.0f, 1.25f, glm::vec4(0.9f, 0.9f, 0.9f, 0.95f));
    drawBar(barX, hopeY, barW, barH, meters.hope, hopeCol, bgBarCol, borderCol);

    // --- 2. Top-Right Inventory & Equipment Status ---
    float eqPanelW = 260.0f;
    float eqPanelH = 65.0f;
    float eqX = (float)screenW_ - eqPanelW - 24.0f;
    float eqY = 24.0f;

    drawRect(eqX, eqY, eqPanelW, eqPanelH, glm::vec4(0.04f, 0.06f, 0.09f, 0.82f));
    drawRect(eqX - 1.0f, eqY - 1.0f, eqPanelW + 2.0f, 1.0f, glm::vec4(0.25f, 0.35f, 0.45f, 0.6f));
    drawRect(eqX - 1.0f, eqY + eqPanelH, eqPanelW + 2.0f, 1.0f, glm::vec4(0.25f, 0.35f, 0.45f, 0.6f));

    const char* wNames[] = {"LANTERN [F]", "CLAYMORE [F]", "DB SHOTGUN [F]", "WAR AXE & SHIELD [F]"};
    drawText(wNames[equippedWeapon], eqX + 14.0f, eqY + 14.0f, 1.35f, glm::vec4(0.95f, 0.85f, 0.65f, 1.0f));

    std::string lightStr = std::string("LANTERN: ") + (lanternOn ? "BURNING [L]" : "DARKNESS [L]");
    glm::vec4 lightStrCol = lanternOn ? glm::vec4(1.0f, 0.78f, 0.35f, 1.0f) : glm::vec4(0.5f, 0.5f, 0.55f, 0.8f);
    drawText(lightStr, eqX + 14.0f, eqY + 38.0f, 1.15f, lightStrCol);

    // --- 3. Mauli Red Thread Compass Indicator (Center Bottom) ---
    if (meters.redThreadGlow > 0.05f) {
        float threadAlpha = meters.redThreadGlow;
        float thW = 320.0f;
        float thH = 28.0f;
        float thX = ((float)screenW_ - thW) * 0.5f;
        float thY = (float)screenH_ - 80.0f;

        drawRect(thX, thY, thW, thH, glm::vec4(0.25f, 0.04f, 0.03f, 0.75f * threadAlpha));
        drawText("MAULI RED THREAD: GUIDING EAST", thX + 24.0f, thY + 8.0f, 1.3f, glm::vec4(1.0f, 0.35f, 0.28f, threadAlpha));
    }

    // --- 4. Level & Day Heading Banner (Top Center) ---
    float bannerW = 380.0f;
    float bannerX = ((float)screenW_ - bannerW) * 0.5f;
    float bannerY = 16.0f;
    drawText("THE SEVENTH NIGHT", bannerX + 55.0f, bannerY, 1.75f, glm::vec4(0.92f, 0.94f, 0.98f, 0.9f));
    drawText("ACT I - DAY 1: KAALVAN FOREST", bannerX + 42.0f, bannerY + 20.0f, 1.15f, glm::vec4(0.6f, 0.7f, 0.8f, 0.75f));

    // --- 5. Controls Hints (Bottom Center) ---
    float hintY = (float)screenH_ - 30.0f;
    drawText("A/D: Move | W: Climb / Ledge | Ctrl: Steady Breath | R: Red Thread | X: Action",
             ((float)screenW_ - 660.0f) * 0.5f, hintY, 1.1f, glm::vec4(0.55f, 0.65f, 0.75f, 0.65f));

    end();
}
