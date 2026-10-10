#include "SkySystem.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Sky shader error: %s\n", log);
    }
    return s;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

// Sky Shader
static const char* kSkyVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uModel;
out vec2 vUV;
out vec3 vWorldPos;
void main() {
    vUV = aUV;
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    gl_Position = uProj * uView * wp;
}
)";

static const char* kSkyFS = R"(
#version 330 core
in vec2 vUV;
in vec3 vWorldPos;
uniform float uTime;
out vec4 frag;
void main() {
    // Atmospheric midnight gradient (Sahyadri night)
    vec3 zenith = vec3(0.015, 0.035, 0.075);     // Deep midnight obsidian
    vec3 midSky = vec3(0.055, 0.095, 0.175);     // Sapphire twilight
    vec3 horizon = vec3(0.115, 0.160, 0.235);    // Misty blue-grey horizon haze
    
    float t = clamp(vUV.y, 0.0, 1.0);
    vec3 col = mix(horizon, midSky, smoothstep(0.0, 0.45, t));
    col = mix(col, zenith, smoothstep(0.40, 1.0, t));
    
    // Subtle animated horizontal atmospheric mist bands
    float mist = sin(vWorldPos.x * 0.03 + uTime * 0.08) * cos(vWorldPos.y * 0.08 - uTime * 0.05);
    col += vec3(0.025, 0.035, 0.05) * max(0.0, mist) * (1.0 - t);
    
    frag = vec4(col, 1.0);
}
)";

// Moon & Halo Shader
static const char* kMoonFS = R"(
#version 330 core
in vec2 vUV;
uniform float uTime;
out vec4 frag;
void main() {
    vec2 p = vUV * 2.0 - 1.0;
    float dist = length(p);
    
    // Moon body
    float moonMask = smoothstep(0.48, 0.46, dist);
    
    // Craters / surface texture
    float craters = sin(p.x * 12.0) * cos(p.y * 14.0) * 0.06;
    vec3 moonSurface = vec3(0.96, 0.94, 0.88) + craters;
    
    // Luminous lunar halo / corona
    float halo = exp(-dist * 3.8) * 0.65;
    vec3 haloCol = vec3(0.45, 0.62, 0.88) * halo;
    
    vec3 col = moonSurface * moonMask + haloCol * (1.0 - moonMask * 0.5);
    float alpha = max(moonMask, halo * 0.9);
    
    if (alpha < 0.01) discard;
    frag = vec4(col, alpha);
}
)";

// Mountain & Star Shader
static const char* kMountainFS = R"(
#version 330 core
in vec2 vUV;
in vec3 vWorldPos;
uniform vec3 uColor;
uniform vec3 uFogColor;
uniform float uMistFactor;
out vec4 frag;
void main() {
    float heightT = clamp(vUV.y, 0.0, 1.0);
    // Base is mistier, peak is crisper against the sky
    vec3 col = mix(uFogColor, uColor, smoothstep(0.0, 0.75, heightT));
    frag = vec4(col, 1.0);
}
)";

static const char* kStarFS = R"(
#version 330 core
in vec2 vUV;
uniform float uTwinkle;
out vec4 frag;
void main() {
    float d = length(vUV * 2.0 - 1.0);
    float star = smoothstep(1.0, 0.0, d);
    vec3 col = vec3(0.92, 0.95, 1.0) * star * uTwinkle;
    if (star < 0.05) discard;
    frag = vec4(col, star);
}
)";

} // namespace

SkySystem::~SkySystem() {
    if (skyShader_) glDeleteProgram(skyShader_);
    if (mountainShader_) glDeleteProgram(mountainShader_);
    if (moonShader_) glDeleteProgram(moonShader_);
    if (starShader_) glDeleteProgram(starShader_);
    if (skyVao_) glDeleteVertexArrays(1, &skyVao_);
    if (skyVbo_) glDeleteBuffers(1, &skyVbo_);
    if (moonVao_) glDeleteVertexArrays(1, &moonVao_);
    if (moonVbo_) glDeleteBuffers(1, &moonVbo_);
    if (starsVao_) glDeleteVertexArrays(1, &starsVao_);
    if (starsVbo_) glDeleteBuffers(1, &starsVbo_);
    if (farRidgeVao_) glDeleteVertexArrays(1, &farRidgeVao_);
    if (farRidgeVbo_) glDeleteBuffers(1, &farRidgeVbo_);
    if (midRidgeVao_) glDeleteVertexArrays(1, &midRidgeVao_);
    if (midRidgeVbo_) glDeleteBuffers(1, &midRidgeVbo_);
}

void SkySystem::init() {
    skyShader_ = linkProgram(kSkyVS, kSkyFS);
    mountainShader_ = linkProgram(kSkyVS, kMountainFS);
    moonShader_ = linkProgram(kSkyVS, kMoonFS);
    starShader_ = linkProgram(kSkyVS, kStarFS);

    initSkyDome();
    initMoon();
    initStars();
    initMountains();
}

void SkySystem::initSkyDome() {
    // Massive panoramic sky backdrop canvas placed far back
    float w = 450.0f, h = 120.0f, yBottom = -10.0f;
    std::vector<float> verts = {
        -w, yBottom,     0.0f,  0.0f, 0.0f,
         w, yBottom,     0.0f,  1.0f, 0.0f,
         w, yBottom + h, 0.0f,  1.0f, 1.0f,
        -w, yBottom,     0.0f,  0.0f, 0.0f,
         w, yBottom + h, 0.0f,  1.0f, 1.0f,
        -w, yBottom + h, 0.0f,  0.0f, 1.0f
    };
    skyVertexCount_ = 6;

    glGenVertexArrays(1, &skyVao_);
    glGenBuffers(1, &skyVbo_);
    glBindVertexArray(skyVao_);
    glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void SkySystem::initMoon() {
    // Full moon billboard (quad)
    float s = 14.0f;
    std::vector<float> verts = {
        -s, -s, 0.0f,  0.0f, 0.0f,
         s, -s, 0.0f,  1.0f, 0.0f,
         s,  s, 0.0f,  1.0f, 1.0f,
        -s, -s, 0.0f,  0.0f, 0.0f,
         s,  s, 0.0f,  1.0f, 1.0f,
        -s,  s, 0.0f,  0.0f, 1.0f
    };
    moonVertexCount_ = 6;

    glGenVertexArrays(1, &moonVao_);
    glGenBuffers(1, &moonVbo_);
    glBindVertexArray(moonVao_);
    glBindBuffer(GL_ARRAY_BUFFER, moonVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void SkySystem::initStars() {
    // 140 scattered twinkling stars
    std::vector<float> starVerts;
    std::srand(1337);

    for (int i = 0; i < 140; ++i) {
        float x = -200.0f + ((float)std::rand() / RAND_MAX) * 400.0f;
        float y = 14.0f + ((float)std::rand() / RAND_MAX) * 42.0f;
        float s = 0.25f + ((float)std::rand() / RAND_MAX) * 0.35f;

        // Quad for each star
        starVerts.insert(starVerts.end(), {
            x - s, y - s, 0.0f,  0.0f, 0.0f,
            x + s, y - s, 0.0f,  1.0f, 0.0f,
            x + s, y + s, 0.0f,  1.0f, 1.0f,
            x - s, y - s, 0.0f,  0.0f, 0.0f,
            x + s, y + s, 0.0f,  1.0f, 1.0f,
            x - s, y + s, 0.0f,  0.0f, 1.0f
        });
    }
    starCount_ = (GLsizei)(starVerts.size() / 5);

    glGenVertexArrays(1, &starsVao_);
    glGenBuffers(1, &starsVbo_);
    glBindVertexArray(starsVao_);
    glBindBuffer(GL_ARRAY_BUFFER, starsVbo_);
    glBufferData(GL_ARRAY_BUFFER, starVerts.size() * sizeof(float), starVerts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

static void generateMountainRidge(float xStart, float xEnd, float step,
                                  float baseY, float heightScale,
                                  float freq1, float freq2, float freq3,
                                  std::vector<float>& outVerts) {
    float prevX = xStart;
    auto getH = [&](float x) {
        float h = std::sin(x * freq1) * 0.55f +
                  std::sin(x * freq2 + 1.2f) * 0.30f +
                  std::cos(x * freq3 + 2.7f) * 0.15f;
        return baseY + (h + 1.0f) * heightScale;
    };

    float prevY = getH(prevX);

    for (float x = xStart + step; x <= xEnd; x += step) {
        float currY = getH(x);

        // Quad slice from baseY to mountain peak
        // Triangle 1
        outVerts.insert(outVerts.end(), {prevX, baseY, 0.0f, 0.0f, 0.0f});
        outVerts.insert(outVerts.end(), {x,     baseY, 0.0f, 1.0f, 0.0f});
        outVerts.insert(outVerts.end(), {x,     currY, 0.0f, 1.0f, 1.0f});

        // Triangle 2
        outVerts.insert(outVerts.end(), {prevX, baseY, 0.0f, 0.0f, 0.0f});
        outVerts.insert(outVerts.end(), {x,     currY, 0.0f, 1.0f, 1.0f});
        outVerts.insert(outVerts.end(), {prevX, prevY, 0.0f, 0.0f, 1.0f});

        prevX = x;
        prevY = currY;
    }
}

void SkySystem::initMountains() {
    // 1. Far Mountain Peaks (Jagged Sahyadri Ghats)
    std::vector<float> farVerts;
    generateMountainRidge(-250.0f, 250.0f, 2.5f, -2.0f, 18.0f, 0.018f, 0.045f, 0.11f, farVerts);
    farRidgeCount_ = (GLsizei)(farVerts.size() / 5);

    glGenVertexArrays(1, &farRidgeVao_);
    glGenBuffers(1, &farRidgeVbo_);
    glBindVertexArray(farRidgeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, farRidgeVbo_);
    glBufferData(GL_ARRAY_BUFFER, farVerts.size() * sizeof(float), farVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // 2. Mid Mountain Rolling Ridge
    std::vector<float> midVerts;
    generateMountainRidge(-220.0f, 220.0f, 2.0f, -1.0f, 10.5f, 0.032f, 0.075f, 0.16f, midVerts);
    midRidgeCount_ = (GLsizei)(midVerts.size() / 5);

    glGenVertexArrays(1, &midRidgeVao_);
    glGenBuffers(1, &midRidgeVbo_);
    glBindVertexArray(midRidgeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, midRidgeVbo_);
    glBufferData(GL_ARRAY_BUFFER, midVerts.size() * sizeof(float), midVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void SkySystem::update(float dt) {
    time_ += dt;
    cloudScroll_ += dt * 0.8f;
}

void SkySystem::draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 1. Draw Panoramic Sky Gradient (follows camera X to stay seamless)
    glUseProgram(skyShader_);
    glUniformMatrix4fv(glGetUniformLocation(skyShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(skyShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniform1f(glGetUniformLocation(skyShader_, "uTime"), time_);

    glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), glm::vec3(camPos.x, 0.0f, -75.0f));
    glUniformMatrix4fv(glGetUniformLocation(skyShader_, "uModel"), 1, GL_FALSE, &skyModel[0][0]);
    glBindVertexArray(skyVao_);
    glDrawArrays(GL_TRIANGLES, 0, skyVertexCount_);

    // 2. Draw Stars (twinkling)
    glUseProgram(starShader_);
    glUniformMatrix4fv(glGetUniformLocation(starShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(starShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);

    float twinkle = 0.75f + 0.25f * std::sin(time_ * 3.2f);
    glUniform1f(glGetUniformLocation(starShader_, "uTwinkle"), twinkle);

    glm::mat4 starsModel = glm::translate(glm::mat4(1.0f), glm::vec3(camPos.x * 0.95f, 0.0f, -70.0f));
    glUniformMatrix4fv(glGetUniformLocation(starShader_, "uModel"), 1, GL_FALSE, &starsModel[0][0]);
    glBindVertexArray(starsVao_);
    glDrawArrays(GL_TRIANGLES, 0, starCount_);

    // 3. Draw Full Moon with Halo
    glUseProgram(moonShader_);
    glUniformMatrix4fv(glGetUniformLocation(moonShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(moonShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniform1f(glGetUniformLocation(moonShader_, "uTime"), time_);

    // Moon position in upper sky over the forest
    glm::vec3 moonPos = glm::vec3(camPos.x * 0.92f + 25.0f, 22.0f, -65.0f);
    glm::mat4 moonModel = glm::translate(glm::mat4(1.0f), moonPos);
    glUniformMatrix4fv(glGetUniformLocation(moonShader_, "uModel"), 1, GL_FALSE, &moonModel[0][0]);
    glBindVertexArray(moonVao_);
    glDrawArrays(GL_TRIANGLES, 0, moonVertexCount_);

    // 4. Draw Far Mountain Ridge (Parallax 0.12x)
    glUseProgram(mountainShader_);
    glUniformMatrix4fv(glGetUniformLocation(mountainShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(mountainShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);

    glm::vec3 farColor(0.065f, 0.095f, 0.155f);
    glm::vec3 farFog(0.115f, 0.160f, 0.235f);
    glUniform3fv(glGetUniformLocation(mountainShader_, "uColor"), 1, &farColor[0]);
    glUniform3fv(glGetUniformLocation(mountainShader_, "uFogColor"), 1, &farFog[0]);

    float farParallaxX = camPos.x * 0.88f; // Moves slowly with camera
    glm::mat4 farModel = glm::translate(glm::mat4(1.0f), glm::vec3(farParallaxX, 0.0f, -55.0f));
    glUniformMatrix4fv(glGetUniformLocation(mountainShader_, "uModel"), 1, GL_FALSE, &farModel[0][0]);
    glBindVertexArray(farRidgeVao_);
    glDrawArrays(GL_TRIANGLES, 0, farRidgeCount_);

    // 5. Draw Mid Mountain Rolling Hills (Parallax 0.28x)
    glm::vec3 midColor(0.042f, 0.065f, 0.105f);
    glm::vec3 midFog(0.085f, 0.120f, 0.180f);
    glUniform3fv(glGetUniformLocation(mountainShader_, "uColor"), 1, &midColor[0]);
    glUniform3fv(glGetUniformLocation(mountainShader_, "uFogColor"), 1, &midFog[0]);

    float midParallaxX = camPos.x * 0.72f;
    glm::mat4 midModel = glm::translate(glm::mat4(1.0f), glm::vec3(midParallaxX, 0.0f, -38.0f));
    glUniformMatrix4fv(glGetUniformLocation(mountainShader_, "uModel"), 1, GL_FALSE, &midModel[0][0]);
    glBindVertexArray(midRidgeVao_);
    glDrawArrays(GL_TRIANGLES, 0, midRidgeCount_);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
