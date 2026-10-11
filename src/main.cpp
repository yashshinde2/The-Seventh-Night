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
#include <map>

#include "Model.h"
#include "core/Assets.h"
#include "anim/SkinnedModel.h"
#include "anim/Animation.h"
#include "anim/Animator.h"
#include "physics/PhysicsEngine.h"
#include "game/GameMeters.h"
#include "environment/SkySystem.h"

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

    // Alpha cutout for leaves, ferns, vines, fences
    if (base.a < 0.25) discard;

    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;

    // Rich midnight ambient and cool directional moonlight
    vec3 ambient = vec3(0.32, 0.38, 0.46);
    vec3 moonDir = normalize(vec3(-0.4, 1.0, 0.6));
    float diff = max(dot(n, moonDir), 0.0);
    vec3 moon = vec3(0.42, 0.54, 0.72) * diff;

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
    float fog = 1.0 - exp(-pow(fd * 0.018, 1.35));
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
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
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

        // --- Phase 1: Robust Asset Discovery ---
        fs::path assetsRoot = AssetManager::findAssetsRoot(argv[0]);
        AssetManager::printDiscoveredAssets(assetsRoot);

        // Model storage and named lookup
        std::vector<Model> propTemplates;
        std::map<std::string, size_t> modelMap;

        auto loadProp = [&](const std::string& name, const fs::path& p) {
            Model m;
            bool ok = m.loadGLB(p.string());
            if (ok) {
                modelMap[name] = propTemplates.size();
                propTemplates.push_back(std::move(m));
                std::printf("[Asset] Loaded %-28s -> SUCCESS\n", name.c_str());
            } else {
                std::printf("[Asset] Loaded %-28s -> FAILED\n", name.c_str());
            }
        };

        // 1. Medieval Village Assets
        fs::path villageDir = assetsRoot / "Medieval Village MegaKit[Standard]" / "Medieval Village MegaKit[Standard]" / "glTF";
        loadProp("wagon", villageDir / "Prop_Wagon.gltf");
        loadProp("crate", villageDir / "Prop_Crate.gltf");
        loadProp("fenceSingle", villageDir / "Prop_WoodenFence_Single.gltf");
        loadProp("fenceExt", villageDir / "Prop_WoodenFence_Extension1.gltf");
        loadProp("chimney", villageDir / "Prop_Chimney.gltf");
        loadProp("wallPlasterDoor", villageDir / "Wall_Plaster_Door_Round.gltf");
        loadProp("wallPlasterStraight", villageDir / "Wall_Plaster_Straight.gltf");
        loadProp("wallPlasterWindow", villageDir / "Wall_Plaster_Window_Wide_Flat.gltf");
        loadProp("wallBrickStraight", villageDir / "Wall_UnevenBrick_Straight.gltf");
        loadProp("roof4x6", villageDir / "Roof_RoundTiles_4x6.gltf");
        loadProp("roof4x4", villageDir / "Roof_RoundTiles_4x4.gltf");
        loadProp("roofFront", villageDir / "Roof_Front_Brick4.gltf");
        loadProp("cornerBrick", villageDir / "Corner_Exterior_Brick.gltf");

        // 2. Downtown City Assets
        fs::path cityDir = assetsRoot / "Downtown City MegaKit[Standard]" / "Exports" / "glTF (Godot)";
        loadProp("cityBldSmall", cityDir / "Building_Small_1.gltf");
        loadProp("cityBldMed", cityDir / "Building_Medium_2_001.gltf");
        loadProp("cityBldLarge", cityDir / "Building_Large_2.gltf");
        loadProp("cityStreet", cityDir / "Street_2Lane.gltf");
        loadProp("citySidewalk", cityDir / "Sidewalk_Straight_3m.gltf");

        // 3. Stylized Nature Assets
        fs::path natureDir = assetsRoot / "Stylized Nature MegaKit[Standard]" / "glTF";
        loadProp("treeCommon1", natureDir / "CommonTree_1.gltf");
        loadProp("treeCommon2", natureDir / "CommonTree_2.gltf");
        loadProp("treeDead1", natureDir / "DeadTree_1.gltf");
        loadProp("treeDead2", natureDir / "DeadTree_2.gltf");
        loadProp("treeTwisted", natureDir / "TwistedTree_1.gltf");
        loadProp("rock1", natureDir / "Rock_Medium_1.gltf");
        loadProp("rock2", natureDir / "Rock_Medium_2.gltf");
        loadProp("bush", natureDir / "Bush_Common.gltf");
        loadProp("fern", natureDir / "Fern_1.gltf");
        loadProp("mushroom", natureDir / "Mushroom_Common.gltf");
        loadProp("rockPath", natureDir / "RockPath_Round_Wide.gltf");
        loadProp("grassShort", natureDir / "Grass_Common_Short.gltf");
        loadProp("grassTall", natureDir / "Grass_Wispy_Tall.gltf");
        loadProp("flowerGroup", natureDir / "Flower_3_Group.gltf");
        loadProp("pine", natureDir / "Pine_1.gltf");
        loadProp("pebble", natureDir / "Pebble_Round_1.gltf");

        // Additional CLI models if provided
        for (int i = 1; i < argc; ++i) {
            Model m;
            if (m.loadGLB(argv[i])) propTemplates.push_back(std::move(m));
        }

        std::vector<LevelProp> worldProps;
        auto placeProp = [&](const std::string& name, glm::vec3 pos, float scale = 1.0f, float rotY = 0.0f) {
            auto it = modelMap.find(name);
            if (it != modelMap.end()) {
                worldProps.push_back({it->second, pos, scale, rotY});
            }
        };

        // --- Assemble World Across Distinct Narrative Zones ---

        // ==========================================
        // ZONE 1: Western Industrial City Outskirts (X: -120 to -30)
        // ==========================================
        // Street paving and sidewalks along ground
        for (float x = -115.0f; x <= -30.0f; x += 12.0f) {
            placeProp("cityStreet", glm::vec3(x, 0.0f, 0.0f), 1.0f, 0.0f);
            placeProp("citySidewalk", glm::vec3(x, 0.0f, 3.2f), 1.0f, 0.0f);
        }

        // City skyline background architecture
        placeProp("cityBldLarge", glm::vec3(-112.0f, 0.0f, -6.5f), 1.0f, 0.0f);
        placeProp("cityBldMed",   glm::vec3(-95.0f,  0.0f, -5.5f), 1.0f, 0.0f);
        placeProp("cityBldLarge", glm::vec3(-78.0f,  0.0f, -6.0f), 1.1f, 180.0f);
        placeProp("cityBldMed",   glm::vec3(-62.0f,  0.0f, -5.0f), 1.0f, 0.0f);
        placeProp("cityBldSmall", glm::vec3(-48.0f,  0.0f, -4.5f), 1.0f, 0.0f);
        placeProp("cityBldSmall", glm::vec3(-36.0f,  0.0f, -4.0f), 0.95f, 0.0f);

        // Urban transition props (crates, encroaching dead trees, weeds, and pebbles)
        for (float x = -105.0f; x <= -35.0f; x += 16.0f) {
            placeProp("crate", glm::vec3(x + 2.0f, 0.0f, 2.2f), 1.2f, 15.0f);
            placeProp("crate", glm::vec3(x + 2.8f, 0.0f, 2.0f), 1.0f, 40.0f);
            placeProp("crate", glm::vec3(x + 2.4f, 0.9f, 2.1f), 0.9f, -10.0f);
            placeProp("treeDead1", glm::vec3(x - 5.0f, 0.0f, -3.2f), 0.9f, 30.0f);
            placeProp("grassShort", glm::vec3(x - 2.0f, 0.0f, 2.8f), 0.8f, x * 7.0f);
            placeProp("pebble", glm::vec3(x + 4.0f, 0.0f, 1.2f), 1.0f, x * 13.0f);
        }

        // ==========================================
        // ZONE 2: The Medieval Village of Kaalvan (X: -28 to +28)
        // ==========================================
        // Cobblestone stone path connecting the village cottages
        for (float x = -26.0f; x <= 26.0f; x += 3.8f) {
            placeProp("rockPath", glm::vec3(x, 0.015f, 0.0f), 1.15f, (float)((int)(x * 5) % 180));
            placeProp("grassShort", glm::vec3(x + 1.2f, 0.0f, -1.5f), 0.9f, x * 17.0f);
            placeProp("pebble", glm::vec3(x - 1.0f, 0.0f, 1.4f), 1.1f, x * 23.0f);
        }
        for (float x = -20.0f; x <= 20.0f; x += 5.5f) {
            placeProp("flowerGroup", glm::vec3(x, 0.0f, 1.8f), 0.85f, x * 35.0f);
        }

        // Cottage 1: Mill Outpost & Blacksmith (X = -20)
        placeProp("wallPlasterDoor",    glm::vec3(-20.0f, 0.0f, -3.2f), 1.0f, 0.0f);
        placeProp("wallPlasterWindow",  glm::vec3(-18.0f, 0.0f, -3.2f), 1.0f, 0.0f);
        placeProp("wallPlasterStraight",glm::vec3(-22.0f, 0.0f, -3.2f), 1.0f, 0.0f);
        placeProp("roof4x6",            glm::vec3(-20.0f, 2.4f, -3.2f), 1.05f, 0.0f);
        placeProp("chimney",            glm::vec3(-18.5f, 3.8f, -3.2f), 1.0f, 0.0f);
        placeProp("wagon",              glm::vec3(-16.5f, 0.0f, -1.2f), 1.0f, -25.0f);
        placeProp("crate",              glm::vec3(-22.5f, 0.0f, -1.5f), 1.1f, 15.0f);
        placeProp("crate",              glm::vec3(-21.8f, 0.0f, -1.6f), 1.0f, 30.0f);
        placeProp("crate",              glm::vec3(-22.2f, 0.9f, -1.5f), 0.8f, 5.0f);

        // Wooden fences lining the village path
        for (float x = -26.0f; x <= -14.0f; x += 3.0f) {
            placeProp("fenceSingle", glm::vec3(x, 0.0f, 2.8f), 1.0f, 0.0f);
        }

        // Cottage 2: Village Elder's Hearth (X = -5)
        placeProp("wallBrickStraight",  glm::vec3(-7.0f, 0.0f, -3.4f), 1.0f, 0.0f);
        placeProp("wallPlasterDoor",    glm::vec3(-5.0f, 0.0f, -3.4f), 1.0f, 0.0f);
        placeProp("wallPlasterWindow",  glm::vec3(-3.0f, 0.0f, -3.4f), 1.0f, 0.0f);
        placeProp("roof4x6",            glm::vec3(-5.0f, 2.4f, -3.4f), 1.05f, 0.0f);
        placeProp("chimney",            glm::vec3(-3.5f, 3.8f, -3.4f), 1.0f, 0.0f);
        placeProp("fenceExt",           glm::vec3(-8.5f, 0.0f, -1.8f), 1.0f, 10.0f);
        placeProp("fenceSingle",        glm::vec3(-1.5f, 0.0f, -1.8f), 1.0f, -10.0f);
        placeProp("wagon",              glm::vec3(-8.0f, 0.0f, -0.8f), 1.0f, 15.0f);

        // Village Center & Sacred Shrine (X = +6)
        placeProp("cornerBrick", glm::vec3(4.5f, 0.0f, -2.6f), 1.0f, 45.0f);
        placeProp("cornerBrick", glm::vec3(7.5f, 0.0f, -2.6f), 1.0f, -45.0f);
        placeProp("rock1",       glm::vec3(6.0f, 0.0f, -2.2f), 1.2f, 0.0f);
        placeProp("crate",       glm::vec3(8.5f, 0.0f, -1.2f), 1.0f, 10.0f);
        placeProp("crate",       glm::vec3(9.2f, 0.0f, -1.4f), 1.0f, 35.0f);
        placeProp("fern",        glm::vec3(5.0f, 0.0f, 1.2f), 1.0f, 0.0f);
        placeProp("fern",        glm::vec3(7.0f, 0.0f, 1.4f), 1.1f, 45.0f);

        // Cottage 3: Forest Edge Gatehouse (X = +18)
        placeProp("wallPlasterDoor",    glm::vec3(18.0f, 0.0f, -3.2f), 1.0f, 0.0f);
        placeProp("wallBrickStraight",  glm::vec3(20.0f, 0.0f, -3.2f), 1.0f, 0.0f);
        placeProp("roof4x4",            glm::vec3(19.0f, 2.4f, -3.2f), 1.05f, 0.0f);
        placeProp("chimney",            glm::vec3(19.8f, 3.8f, -3.2f), 1.0f, 0.0f);
        placeProp("wagon",              glm::vec3(15.0f, 0.0f, -1.2f), 1.0f, -10.0f);
        for (float x = 12.0f; x <= 26.0f; x += 3.0f) {
            placeProp("fenceSingle", glm::vec3(x, 0.0f, 2.8f), 1.0f, 0.0f);
        }

        // ==========================================
        // ZONE 3: Ancient Kaalvan Deep Forest (X: +28 to +120)
        // ==========================================
        // Background dense forest canopy (z = -3.5 to -6.5)
        for (float x = 28.0f; x <= 120.0f; x += 4.5f) {
            std::string tName = ((int)x % 3 == 0) ? "treeCommon1" : (((int)x % 3 == 1) ? "treeCommon2" : "treeTwisted");
            float scale = 0.85f + 0.35f * std::sin(x * 0.4f);
            float rot = (float)((int)(x * 19.0f) % 360);
            placeProp(tName, glm::vec3(x, 0.0f, -4.2f + 1.2f * std::sin(x * 0.2f)), scale, rot);
            if ((int)x % 9 == 0) {
                placeProp("treeDead2", glm::vec3(x + 2.0f, 0.0f, -5.5f), 1.1f, rot + 45.0f);
            }
            if ((int)x % 7 == 0) {
                placeProp("pine", glm::vec3(x - 1.5f, 0.0f, -6.5f), 1.3f, rot + 90.0f);
            }
        }

        // Midground rocks, ferns, mushrooms, and bushes
        for (float x = 32.0f; x <= 118.0f; x += 7.0f) {
            placeProp("rock1", glm::vec3(x, 0.0f, -0.8f + 0.6f * std::sin(x)), 0.9f, x * 15.0f);
            placeProp("bush",  glm::vec3(x + 2.5f, 0.0f, 0.2f), 1.0f, x * 30.0f);
            placeProp("mushroom", glm::vec3(x - 1.2f, 0.0f, -0.3f), 1.2f, x * 12.0f);
            placeProp("grassTall", glm::vec3(x + 1.0f, 0.0f, -0.5f), 1.0f, x * 21.0f);
        }

        // Foreground silhouette framing (z = 2.4 to 3.2)
        for (float x = 30.0f; x <= 115.0f; x += 11.0f) {
            placeProp("treeTwisted", glm::vec3(x, 0.0f, 3.2f), 1.25f, x * 22.0f);
            placeProp("fern", glm::vec3(x + 1.5f, 0.0f, 2.5f), 1.1f, x * 17.0f);
            placeProp("fern", glm::vec3(x - 1.5f, 0.0f, 2.6f), 1.0f, x * 29.0f);
        }

        // Load Skinned Characters & Animations dynamically from assetsRoot
        fs::path maleCharPath = assetsRoot / "Universal Base Characters[Standard]" / "Universal Base Characters[Standard]" / "Base Characters" / "Godot - UE" / "Superhero_Male_FullBody.gltf";
        fs::path femaleCharPath = assetsRoot / "Universal Base Characters[Standard]" / "Universal Base Characters[Standard]" / "Base Characters" / "Godot - UE" / "Superhero_Female_FullBody.gltf";

        SkinnedModel playerMale;
        bool hasMale = playerMale.loadGLB(maleCharPath.string());
        std::printf("[Character] Load Male Character '%s': %s\n", maleCharPath.filename().string().c_str(), hasMale ? "SUCCESS" : "FAILED");

        SkinnedModel playerFemale;
        bool hasFemale = playerFemale.loadGLB(femaleCharPath.string());
        std::printf("[Character] Load Female Character '%s': %s\n", femaleCharPath.filename().string().c_str(), hasFemale ? "SUCCESS" : "FAILED");

        if (!hasMale && !hasFemale) {
            std::fprintf(stderr, "\n[WARNING] Phase 1: Player character models failed to load! Using fallback blue box.\n");
        }

        int activeCharIdx = 0; // 0 = Male, 1 = Female
        bool lastCKeyState = false;

        fs::path ual1Path = assetsRoot / "Universal Animation Library[Standard]" / "Universal Animation Library[Standard]" / "Unreal-Godot" / "UAL1_Standard.glb";
        fs::path ual2Path = assetsRoot / "Universal Animation Library 2[Standard]" / "Universal Animation Library 2[Standard]" / "Unreal-Godot" / "UAL2_Standard.glb";

        AnimationLibrary animLib;
        bool okAnim1 = animLib.loadGLBAnimations(ual1Path.string());
        std::printf("[Phase 1] Load UAL1 Animations '%s': %s\n", ual1Path.filename().string().c_str(), okAnim1 ? "SUCCESS" : "FAILED");

        bool okAnim2 = animLib.loadGLBAnimations(ual2Path.string());
        std::printf("[Phase 1] Load UAL2 Animations '%s': %s\n", ual2Path.filename().string().c_str(), okAnim2 ? "SUCCESS" : "FAILED");

        animLib.printClipNames();

        Animator animator;
        animator.init(animLib);

        if (hasMale) {
            const AnimationClip* testClip = animLib.findClipAny({"Idle_Loop", "Idle"});
            if (testClip) {
                animLib.checkRetarget(*testClip, playerMale.getBoneNames());
            }
        }

        CharacterPhysics playerPhysics;
        PlayerMeters meters;

        // Realistic Sky, Full Moon, Stars, and Sahyadri Mountains
        SkySystem skySystem;
        skySystem.init();

        // Ground and interactive village/city platforms
        std::vector<ObstaclePlatform> platforms;
        platforms.push_back({AABB{glm::vec3(-150.0f, -1.0f, -10.0f), glm::vec3(150.0f, 0.0f, 10.0f)}, SurfaceType::Normal, false, 1});
        // Wagon platform at X = -16.5
        platforms.push_back({AABB{glm::vec3(-17.5f, 0.0f, -2.0f), glm::vec3(-15.5f, 0.85f, -0.4f)}, SurfaceType::OneWay, false, 1});
        // Crate stack platform at X = -22.0
        platforms.push_back({AABB{glm::vec3(-23.0f, 0.0f, -2.5f), glm::vec3(-21.0f, 0.95f, -0.6f)}, SurfaceType::OneWay, false, 1});
        // Wagon platform at X = 15.0
        platforms.push_back({AABB{glm::vec3(14.0f, 0.0f, -2.0f), glm::vec3(16.0f, 0.85f, -0.4f)}, SurfaceType::OneWay, false, 1});

        float camX = 0.0f;
        double lastTime = glfwGetTime();

        std::printf("\n=== The Seventh Night Engine Initialized ===\n");
        std::printf("Controls:\n");
        std::printf("  A / D (or Left / Right Arrow): Move Left / Right\n");
        std::printf("  W / S (or Up / Down Arrow):    Move Forward / Backward (Depth)\n");
        std::printf("  Space: Jump | Left Shift: Sprint\n");
        std::printf("  Q / E: Lower / Raise Will | Ctrl: Steady Breath | R: Red Thread Sense | Esc: Quit\n");
        std::printf("  C: Switch Active Player Character (Male / Female)\n");
        std::fflush(stdout);

        // Run Phase 0 Physics Test Verification
        runPhysicsPhase0Test();

        while (!glfwWindowShouldClose(window)) {
            double now = glfwGetTime();
            float dt = (float)std::min(now - lastTime, 0.05);
            lastTime = now;

            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

            // Toggle active character with 'C' key
            bool cKeyNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            if (cKeyNow && !lastCKeyState) {
                if (hasMale && hasFemale) {
                    activeCharIdx = 1 - activeCharIdx;
                    std::printf("[Character] Switched active character to: %s\n", activeCharIdx == 0 ? "Male" : "Female");
                }
            }
            lastCKeyState = cKeyNow;

            // Movement & Inputs in all directions
            bool moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
            bool moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
            bool moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
            bool moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
            bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            bool jumpReq = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            bool dropReq = moveBackward && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

            bool steadyBreath = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && !moveLeft && !moveRight && !moveForward && !moveBackward;
            bool redThread = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) meters.will = std::max(0.0f, meters.will - 0.6f * dt);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) meters.will = std::min(1.0f, meters.will + 0.6f * dt);

            // Physics & Meters Update (with 360-degree planar input)
            playerPhysics.update(dt, platforms, moveLeft, moveRight, sprint, jumpReq, dropReq, moveForward, moveBackward);
            meters.update(dt, false, sprint && (moveLeft || moveRight || moveForward || moveBackward), steadyBreath, redThread);

            // Update Living Celestial Environment
            skySystem.update(dt);

            // Determine active and companion characters
            SkinnedModel& activePlayer = (activeCharIdx == 0 && hasMale) ? playerMale : ((hasFemale) ? playerFemale : playerMale);
            bool hasSkinnedPlayer = (activeCharIdx == 0) ? hasMale : hasFemale;

            SkinnedModel* companionChar = nullptr;
            if (activeCharIdx == 0 && hasFemale) {
                companionChar = &playerFemale;
            } else if (activeCharIdx == 1 && hasMale) {
                companionChar = &playerMale;
            }

            // Animation Update (Phase 2 & 3 State Machine, Blending & 360-deg Facing)
            if (hasSkinnedPlayer) {
                animator.update(dt, playerPhysics, activePlayer);
            }

            // Companion character idle animation at Kaalvan village center
            if (companionChar) {
                const AnimationClip* companionClip = animLib.findClipAny({"Idle_Talking_Loop", "Sitting_Idle_Loop", "Idle_Loop", "Idle"});
                if (companionClip) {
                    companionChar->updateSkeleton(companionClip, (float)now, true);
                }
            }

            // Smooth Cinematic Camera Tracking with depth
            camX += (playerPhysics.position.x - camX) * std::min(1.0f, 4.0f * dt);
            glm::vec3 camPos(camX, 2.6f, 10.0f);
            glm::vec3 target(camX, 1.3f, playerPhysics.position.z * 0.35f);
            glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));

            int fbw, fbh;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            if (fbh == 0) fbh = 1;
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)fbw / fbh, 0.1f, 200.0f);

            // Render Pass
            glViewport(0, 0, fbw, fbh);
            glClearColor(0.015f, 0.035f, 0.075f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 1. Draw Celestial Midnight Sky, Stars, Full Moon, and Parallax Sahyadri Mountains
            skySystem.draw(view, proj, camPos);

            // 2. Main 3D World Geometry Pass
            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, &proj[0][0]);
            glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, &camPos[0]);

            glm::vec3 lanternPos = playerPhysics.position + glm::vec3(0.0f, 1.2f, 0.4f);
            glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, &lanternPos[0]);

            float flicker = 0.92f + 0.08f * std::sin((float)now * 17.0f) * std::sin((float)now * 5.3f);
            glUniform1f(glGetUniformLocation(program, "uWill"), meters.will * flicker);
            glUniform1f(glGetUniformLocation(program, "uRedGlow"), meters.redThreadGlow);

            // Draw Ground Plane
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
            ground.draw(program, glm::mat4(1.0f));

            // Draw World Props (City, Village, Forest, Ground paths)
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
            for (const auto& prop : worldProps) {
                if (prop.modelIdx < propTemplates.size()) {
                    glm::mat4 m = glm::translate(glm::mat4(1.0f), prop.pos);
                    m = glm::rotate(m, glm::radians(prop.rotationY), glm::vec3(0, 1, 0));
                    m = glm::scale(m, glm::vec3(prop.scale));
                    propTemplates[prop.modelIdx].draw(program, m);
                }
            }

            // 3. Draw Village Companion Character (Kaalvan Village Shrine at X = 6.0)
            if (companionChar) {
                glm::mat4 compMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -0.6f));
                compMatrix = glm::rotate(compMatrix, glm::radians(180.0f), glm::vec3(0, 1, 0));
                companionChar->draw(program, compMatrix);
            }

            // 4. Draw Active Player Character (Skinned mesh or fallback)
            glm::mat4 playerModelMatrix = glm::translate(glm::mat4(1.0f), playerPhysics.position);
            playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(animator.getFacingAngleDegrees()), glm::vec3(0, 1, 0));
            if (hasSkinnedPlayer) {
                activePlayer.draw(program, playerModelMatrix);
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
