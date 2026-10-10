// The Seventh Night - C++17 & OpenGL 3.3 Engine
// 2.5D Narrative Survival Horror Engine built from Scratch

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Model.h"
#include "anim/SkinnedModel.h"
#include "anim/Animation.h"
#include "physics/PhysicsEngine.h"
#include "game/GameMeters.h"

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

out vec4 frag;

void main() {
    vec4 base = uColor;
    if (uHasTex == 1) base *= texture(uTex, vUV);
    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;

    // Atmospheric midnight ambient and moon lighting
    vec3 ambient = vec3(0.22, 0.28, 0.38);
    vec3 moon = vec3(0.35, 0.45, 0.65) * max(dot(n, normalize(vec3(-0.4, 1.0, 0.6))), 0.0);

    // Warm lantern glow
    vec3 L = uLightPos - vPos;
    float d = length(L);
    L /= d;
    float att = 1.0 / (1.0 + 0.12 * d + 0.015 * d * d);
    vec3 lamp = vec3(1.0, 0.72, 0.35) * max(dot(n, L), 0.0) * att * 3.5 * uWill;

    // Mauli Red Thread glow
    vec3 redLight = vec3(0.95, 0.25, 0.20) * uRedGlow * max(dot(n, L), 0.0) * att * 2.0;

    vec3 col = base.rgb * (ambient + moon + lamp + redLight);

    // Dynamic fog attenuation
    float fd = length(uCamPos - vPos);
    float fog = 1.0 - exp(-pow(fd * 0.022, 1.4));
    vec3 fogColor = mix(vec3(0.08, 0.12, 0.18), vec3(0.30, 0.08, 0.06), uRedGlow * 0.5);
    col = mix(col, fogColor, fog);

    frag = vec4(col, base.a);
}
)";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader error: %s\n", log);
    }
    return s;
}

static GLuint makeProgram() {
    GLuint vs = compile(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Link error: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

struct LevelProp {
    size_t modelIdx;
    glm::vec3 pos;
    float scale = 1.0f;
    float rotationY = 0.0f;
};

int main(int argc, char** argv) {
    if (!glfwInit()) {
        std::fprintf(stderr, "Could not start GLFW\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "The Seventh Night — 2.5D Survival Horror Engine", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Could not create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "Could not start GLEW\n");
        return 1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    GLuint program = makeProgram();

    {
        // Environment Ground
        Model ground = Model::groundQuad(200.0f, glm::vec4(0.18f, 0.28f, 0.18f, 1.0f));
        Model playerFallback = Model::box(glm::vec3(0.6f, 1.7f, 0.4f), glm::vec4(0.25f, 0.45f, 0.85f, 1.0f));

        // Load Nature Props
        std::vector<Model> propTemplates;
        std::vector<std::string> propPaths = {
            "assets/Stylized Nature MegaKit[Standard]/glTF/CommonTree_1.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/CommonTree_2.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/DeadTree_1.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/DeadTree_2.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/TwistedTree_1.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/Rock_Medium_1.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/Rock_Medium_2.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/Bush_Common.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/Fern_1.gltf",
            "assets/Stylized Nature MegaKit[Standard]/glTF/Mushroom_Common.gltf"
        };

        for (const auto& path : propPaths) {
            Model m;
            if (m.loadGLB(path)) {
                propTemplates.push_back(std::move(m));
            }
        }

        // Additional CLI models
        for (int i = 1; i < argc; ++i) {
            Model m;
            if (m.loadGLB(argv[i])) propTemplates.push_back(std::move(m));
        }

        // Generate Dense Forest Level Layout along lanes
        const float laneZ[3] = {2.0f, 0.0f, -2.0f};
        std::vector<LevelProp> worldProps;

        if (!propTemplates.empty()) {
            // Background Lane Trees & Rocks (z = -3.5 to -1.5)
            for (float x = -80.0f; x <= 120.0f; x += 4.5f) {
                size_t modelIdx = (size_t)(std::abs((int)(x * 7.0f)) % propTemplates.size());
                float scale = 0.8f + 0.4f * std::sin(x * 0.3f);
                float rot = (float)((int)(x * 13.0f) % 360);
                worldProps.push_back({modelIdx, glm::vec3(x, 0.0f, laneZ[2] - 1.5f), scale, rot});
            }
            // Middle Lane Rocks & Bushes
            for (float x = -75.0f; x <= 115.0f; x += 8.0f) {
                size_t modelIdx = 5 + (size_t)(std::abs((int)(x * 3.0f)) % 4);
                if (modelIdx < propTemplates.size()) {
                    worldProps.push_back({modelIdx, glm::vec3(x, 0.0f, laneZ[1]), 1.0f, x * 15.0f});
                }
            }
            // Foreground Lane Trees & Ferns (z = 2.5)
            for (float x = -70.0f; x <= 110.0f; x += 11.0f) {
                size_t modelIdx = (size_t)(std::abs((int)(x * 5.0f)) % propTemplates.size());
                worldProps.push_back({modelIdx, glm::vec3(x, 0.0f, laneZ[0] + 1.2f), 1.2f, x * 25.0f});
            }
        }

        // Load Skinned Character & Animations
        SkinnedModel playerCharacter;
        bool hasSkinnedPlayer = playerCharacter.loadGLB("assets/Universal Base Characters[Standard]/Universal Base Characters[Standard]/Base Characters/Godot - UE/Superhero_Male_FullBody.gltf");

        AnimationLibrary animLib;
        animLib.loadGLBAnimations("assets/Universal Animation Library[Standard]/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb");
        animLib.loadGLBAnimations("assets/Universal Animation Library 2[Standard]/Universal Animation Library 2[Standard]/Unreal-Godot/UAL2_Standard.glb");

        const AnimationClip* idleClip = animLib.findClip("Idle");
        const AnimationClip* walkClip = animLib.findClip("Walk");
        const AnimationClip* runClip = animLib.findClip("Sprint");

        CharacterPhysics playerPhysics;
        PlayerMeters meters;

        std::vector<ObstaclePlatform> platforms;
        platforms.push_back({AABB{glm::vec3(-150.0f, -1.0f, -10.0f), glm::vec3(150.0f, 0.0f, 10.0f)}, SurfaceType::Normal, false, 1});

        float animTime = 0.0f;
        float camX = 0.0f;
        double lastTime = glfwGetTime();

        std::printf("\n=== The Seventh Night Engine Initialized ===\n");
        std::printf("Controls:\n");
        std::printf(" A/D: Move | W/S: Change Depth Lane | Space: Jump | Shift: Sprint\n");
        std::printf(" Q/E: Lower/Raise Will | Ctrl: Steady Breath | R: Red Thread Sense | Esc: Quit\n");
        std::fflush(stdout);

        while (!glfwWindowShouldClose(window)) {
            double now = glfwGetTime();
            float dt = (float)std::min(now - lastTime, 0.05);
            lastTime = now;

            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

            // Movement & Inputs
            bool moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            bool moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
            bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            bool jumpReq = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            bool dropReq = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

            static bool wPressed = false, sPressed = false;
            bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !dropReq;
            if (w && !wPressed) playerPhysics.targetLane = std::min(playerPhysics.targetLane + 1, 2);
            if (s && !sPressed) playerPhysics.targetLane = std::max(playerPhysics.targetLane - 1, 0);
            wPressed = w;
            sPressed = s;

            bool steadyBreath = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && !moveLeft && !moveRight;
            bool redThread = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) meters.will = std::max(0.0f, meters.will - 0.6f * dt);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) meters.will = std::min(1.0f, meters.will + 0.6f * dt);

            // Physics & Meters Update
            playerPhysics.update(dt, platforms, moveLeft, moveRight, sprint, jumpReq, dropReq);
            meters.update(dt, false, sprint && (moveLeft || moveRight), steadyBreath, redThread);

            // Animation Update
            animTime += dt;
            const AnimationClip* activeClip = idleClip;
            if (moveLeft || moveRight) {
                activeClip = sprint ? (runClip ? runClip : walkClip) : walkClip;
            }
            if (hasSkinnedPlayer) {
                playerCharacter.updateSkeleton(activeClip, animTime);
            }

            // Smooth Camera Tracking
            camX += (playerPhysics.position.x - camX) * std::min(1.0f, 4.0f * dt);
            glm::vec3 camPos(camX, 2.5f, 9.5f);
            glm::vec3 target(camX, 1.2f, 0.0f);
            glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));

            int fbw, fbh;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            if (fbh == 0) fbh = 1;
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)fbw / fbh, 0.1f, 200.0f);

            // Render Pass
            glViewport(0, 0, fbw, fbh);
            glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, &proj[0][0]);
            glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, &camPos[0]);

            glm::vec3 lanternPos = playerPhysics.position + glm::vec3(0.3f, 1.2f, 0.5f);
            glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, &lanternPos[0]);

            float flicker = 0.92f + 0.08f * std::sin((float)now * 17.0f) * std::sin((float)now * 5.3f);
            glUniform1f(glGetUniformLocation(program, "uWill"), meters.will * flicker);
            glUniform1f(glGetUniformLocation(program, "uRedGlow"), meters.redThreadGlow);

            // Draw Ground
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
            ground.draw(program, glm::mat4(1.0f));

            // Draw World Forest Props
            for (const auto& prop : worldProps) {
                if (prop.modelIdx < propTemplates.size()) {
                    glm::mat4 m = glm::translate(glm::mat4(1.0f), prop.pos);
                    m = glm::rotate(m, glm::radians(prop.rotationY), glm::vec3(0, 1, 0));
                    m = glm::scale(m, glm::vec3(prop.scale));
                    propTemplates[prop.modelIdx].draw(program, m);
                }
            }

            // Draw Player Character (Skinned mesh or fallback)
            glm::mat4 playerModelMatrix = glm::translate(glm::mat4(1.0f), playerPhysics.position);
            if (moveLeft) {
                playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(180.0f), glm::vec3(0, 1, 0));
            }
            if (hasSkinnedPlayer) {
                playerCharacter.draw(program, playerModelMatrix);
            } else {
                glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
                playerFallback.draw(program, playerModelMatrix);
            }

            glfwSwapBuffers(window);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
