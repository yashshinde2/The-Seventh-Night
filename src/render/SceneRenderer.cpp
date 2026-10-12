#include "SceneRenderer.h"
#include <cstdio>
#include <cmath>

static const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aJoints;
layout(location = 4) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat4 uJointMatrices[128];
uniform int uIsSkinned;

out vec3 vPos;
out vec3 vNrm;
out vec2 vUV;

void main() {
    mat4 skinMatrix = mat4(1.0);
    if (uIsSkinned == 1) {
        skinMatrix = aWeights.x * uJointMatrices[int(aJoints.x)] +
                     aWeights.y * uJointMatrices[int(aJoints.y)] +
                     aWeights.z * uJointMatrices[int(aJoints.z)] +
                     aWeights.w * uJointMatrices[int(aJoints.w)];
        if (length(skinMatrix[0]) < 0.001) {
            skinMatrix = mat4(1.0);
        }
    }
    vec4 world = uModel * skinMatrix * vec4(aPos, 1.0);
    vPos = world.xyz;
    vNrm = mat3(transpose(inverse(uModel * skinMatrix))) * aNrm;
    vUV = aUV;
    gl_Position = uProj * uView * world;
}
)";

static const char* kFragmentShader = R"(
#version 330 core
in vec3 vPos;
in vec3 vNrm;
in vec2 vUV;

uniform sampler2D uTex;
uniform int uHasTex;
uniform vec4 uColor;
uniform vec3 uLightPos;   // lantern position
uniform vec3 uCamPos;
uniform float uWill;      // 0..1, scales lantern brightness
uniform float uRedGlow;   // mauli red thread glow factor
uniform int uIsBackdrop;  // 1 for painted VN backgrounds

uniform vec3 uAmbient;
uniform vec3 uMoonDir;
uniform vec3 uMoonColor;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uDesaturate; // 0.0 = Full Color, 1.0 = Ash-Grey world

out vec4 frag;

void main() {
    vec4 base = uColor;
    if (uHasTex == 1) base *= texture(uTex, vUV);

    // Alpha cutout for leaves, ferns, vines, fences
    if (base.a < 0.25) discard;

    // Atmospheric painted visual novel backdrops: preserved with cinematic night tone
    if (uIsBackdrop == 1) {
        vec3 bgCol = base.rgb * 0.92 + vec3(0.015, 0.025, 0.045);
        float bgGrey = dot(bgCol, vec3(0.299, 0.587, 0.114));
        bgCol = mix(bgCol, vec3(bgGrey), uDesaturate);
        frag = vec4(bgCol, base.a);
        return;
    }

    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;

    // Rich ambient and cool directional moonlight
    vec3 ambient = uAmbient;
    vec3 moonDir = normalize(uMoonDir);
    float diff = max(dot(n, moonDir), 0.0);
    vec3 moon = uMoonColor * diff;

    // Specular highlight for cobblestone, metal, and damp wood
    vec3 viewDir = normalize(uCamPos - vPos);
    vec3 halfDir = normalize(moonDir + viewDir);
    float spec = pow(max(dot(n, halfDir), 0.0), 16.0) * 0.12;

    // Warm Aarav lantern glow
    vec3 L = uLightPos - vPos;
    float d = length(L);
    L /= max(d, 0.001);
    float att = 1.0 / (1.0 + 0.08 * d + 0.012 * d * d);
    vec3 lamp = vec3(1.0, 0.76, 0.42) * max(dot(n, L), 0.0) * att * 3.8 * max(uWill, 0.25);

    // Mauli Red Thread glow
    vec3 redLight = vec3(0.95, 0.25, 0.20) * uRedGlow * max(dot(n, L), 0.0) * att * 2.2;

    vec3 col = base.rgb * (ambient + moon + lamp + redLight) + vec3(spec);

    // Dynamic distance mist attenuation
    float fd = length(uCamPos - vPos);
    float fog = 1.0 - exp(-pow(fd * uFogDensity, 1.35));
    vec3 finalFogColor = mix(uFogColor, vec3(0.30, 0.08, 0.06), uRedGlow * 0.5);
    col = mix(col, finalFogColor, fog);

    // Milestone 7: Day 4 Desaturation & Ash-Grey world post-processing
    float grey = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(grey), uDesaturate);

    frag = vec4(col, base.a);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[SceneRenderer] Shader compile error: %s\n", log);
    }
    return s;
}

bool SceneRenderer::init() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[SceneRenderer] Program link error: %s\n", log);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTex"), 0);
    return true;
}

void SceneRenderer::shutdown() {
    if (program != 0) {
        glDeleteProgram(program);
        program = 0;
    }
}

void SceneRenderer::beginFrame(int fbw, int fbh, const glm::vec3& camPos, const glm::vec3& camTarget,
                              const SceneAtmosphere& atmosphere, const glm::vec3& lanternPos,
                              float will, float redGlow, float muzzleFlash, float desaturate) {
    cameraPos = camPos;
    viewMatrix = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));
    float aspect = (fbh > 0) ? (float)fbw / (float)fbh : 16.0f / 9.0f;
    projMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);

    glViewport(0, 0, fbw, fbh);
    glClearColor(0.015f, 0.035f, 0.075f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, &viewMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, &projMatrix[0][0]);
    glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, &cameraPos[0]);
    glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, &lanternPos[0]);

    glUniform3fv(glGetUniformLocation(program, "uAmbient"), 1, &atmosphere.ambient[0]);
    glUniform3fv(glGetUniformLocation(program, "uMoonDir"), 1, &atmosphere.moonDir[0]);
    glUniform3fv(glGetUniformLocation(program, "uMoonColor"), 1, &atmosphere.moonColor[0]);
    glUniform3fv(glGetUniformLocation(program, "uFogColor"), 1, &atmosphere.fogColor[0]);
    glUniform1f(glGetUniformLocation(program, "uFogDensity"), atmosphere.fogDensity);

    float lightWill = will + (muzzleFlash > 0.0f ? 4.5f : 0.0f);
    glUniform1f(glGetUniformLocation(program, "uWill"), lightWill);
    glUniform1f(glGetUniformLocation(program, "uRedGlow"), redGlow);
    glUniform1f(glGetUniformLocation(program, "uDesaturate"), desaturate);
}

void SceneRenderer::drawBackdrops(const std::vector<SceneBackdropInstance>& backdrops) {
    glUniform1i(glGetUniformLocation(program, "uIsBackdrop"), 1);
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);

    for (const auto& b : backdrops) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), b.pos);
        // Cast away constness for draw call
        const_cast<Model&>(b.quadModel).draw(program, m);
    }

    glUniform1i(glGetUniformLocation(program, "uIsBackdrop"), 0);
}

void SceneRenderer::drawGround(Model& ground) {
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
    ground.draw(program, glm::mat4(1.0f));
}

void SceneRenderer::drawSceneProps(const Scene& scene) {
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
    for (const auto& prop : scene.props) {
        if (prop.modelIdx < scene.modelTemplates.size()) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), prop.pos);
            m = glm::rotate(m, glm::radians(prop.rotY), glm::vec3(0, 1, 0));
            m = glm::scale(m, glm::vec3(prop.scale));
            const_cast<Model&>(scene.modelTemplates[prop.modelIdx]).draw(program, m);
        }
    }
}

void SceneRenderer::drawCreatures(const std::map<std::string, Model*>& creatureModels,
                                 const std::vector<SceneCreatureInstance>& creatures, float time) {
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
    for (const auto& c : creatures) {
        auto it = creatureModels.find(c.type);
        if (it != creatureModels.end() && it->second != nullptr) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), c.pos);
            if (c.type == "Imp") {
                m = glm::rotate(m, glm::radians(35.0f + 8.0f * std::sin(time * 2.5f)), glm::vec3(0, 1, 0));
            } else if (c.type == "Puglin") {
                m = glm::rotate(m, glm::radians(-35.0f), glm::vec3(0, 1, 0));
            } else if (c.type == "Skeleton") {
                m = glm::scale(m, glm::vec3(1.2f));
                m = glm::rotate(m, glm::radians(55.0f), glm::vec3(0, 1, 0));
            } else if (c.type == "Dragon") {
                m = glm::scale(m, glm::vec3(2.4f));
                m = glm::rotate(m, glm::radians(210.0f), glm::vec3(0, 1, 0));
            }
            it->second->draw(program, m);
        }
    }
}

void SceneRenderer::drawSkinnedCharacter(SkinnedModel& model, const glm::mat4& transform) {
    model.draw(program, transform);
}

void SceneRenderer::drawStaticModel(Model& model, const glm::mat4& transform) {
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
    model.draw(program, transform);
}
