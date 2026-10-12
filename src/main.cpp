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
#include "audio/AudioEngine.h"

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

out vec4 frag;

void main() {
    vec4 base = uColor;
    if (uHasTex == 1) base *= texture(uTex, vUV);

    // Alpha cutout for leaves, ferns, vines, fences
    if (base.a < 0.25) discard;

    // Atmospheric painted visual novel backdrops: preserved with cinematic night tone
    if (uIsBackdrop == 1) {
        vec3 bgCol = base.rgb * 0.92 + vec3(0.015, 0.025, 0.045);
        frag = vec4(bgCol, base.a);
        return;
    }

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
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTex"), 0);

    {
        // Environment Ground
        Model ground = Model::groundQuad(200.0f, glm::vec4(0.18f, 0.28f, 0.18f, 1.0f));
        Model playerFallback = Model::box(glm::vec3(0.6f, 1.7f, 0.4f), glm::vec4(0.25f, 0.45f, 0.85f, 1.0f));

        // --- Phase 1: Robust Asset Discovery & Visual Novel Night Backdrops ---
        fs::path assetsRoot = AssetManager::findAssetsRoot(argv[0]);
        AssetManager::printDiscoveredAssets(assetsRoot);

        // 1. Panoramic Visual Novel Night Backdrops (Parallax Billboard System at Z = -22.0)
        fs::path bgDir = assetsRoot / "Fantasy_VN_Backgrounds";
        GLuint bgCityTex      = Model::loadTextureFromFile((bgDir / "23_city_gate_night.jpg").string());
        GLuint bgVillageTex   = Model::loadTextureFromFile((bgDir / "04_village_square_night.jpg").string());
        GLuint bgForestTex    = Model::loadTextureFromFile((bgDir / "03_forest_path_night.jpg").string());
        GLuint bgGraveyardTex = Model::loadTextureFromFile((bgDir / "15_misty_graveyard_night.jpg").string());

        Model bgCity      = Model::backdropQuad(85.0f, 38.0f, bgCityTex);
        Model bgVillage   = Model::backdropQuad(85.0f, 38.0f, bgVillageTex);
        Model bgForest    = Model::backdropQuad(85.0f, 38.0f, bgForestTex);
        Model bgGraveyard = Model::backdropQuad(85.0f, 38.0f, bgGraveyardTex);
        std::printf("[Backdrops] Loaded 4 Atmospheric VN Night Horizons (City, Village, Forest, Graveyard)\n");

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

        // 2. Medieval Village Assets
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

        // 3. Downtown City Assets
        fs::path cityDir = assetsRoot / "Downtown City MegaKit[Standard]" / "Exports" / "glTF (Godot)";
        loadProp("cityBldSmall", cityDir / "Building_Small_1.gltf");
        loadProp("cityBldMed",   cityDir / "Building_Medium_2_001.gltf");
        loadProp("cityBldLarge", cityDir / "Building_Large_2.gltf");
        loadProp("cityStreet",   cityDir / "Street_2Lane.gltf");
        loadProp("citySidewalk", cityDir / "Sidewalk_Straight_3m.gltf");

        // 4. Stylized Nature Assets
        fs::path natureDir = assetsRoot / "Stylized Nature MegaKit[Standard]" / "glTF";
        loadProp("treeCommon1", natureDir / "CommonTree_1.gltf");
        loadProp("treeCommon2", natureDir / "CommonTree_2.gltf");
        loadProp("treeDead1",   natureDir / "DeadTree_1.gltf");
        loadProp("treeDead2",   natureDir / "DeadTree_2.gltf");
        loadProp("treeTwisted", natureDir / "TwistedTree_1.gltf");
        loadProp("rock1",       natureDir / "Rock_Medium_1.gltf");
        loadProp("rock2",       natureDir / "Rock_Medium_2.gltf");
        loadProp("bush",        natureDir / "Bush_Common.gltf");
        loadProp("fern",        natureDir / "Fern_1.gltf");
        loadProp("mushroom",    natureDir / "Mushroom_Common.gltf");
        loadProp("rockPath",    natureDir / "RockPath_Round_Wide.gltf");
        loadProp("grassShort",  natureDir / "Grass_Common_Short.gltf");
        loadProp("grassTall",   natureDir / "Grass_Wispy_Tall.gltf");
        loadProp("flowerGroup", natureDir / "Flower_3_Group.gltf");
        loadProp("pine",        natureDir / "Pine_1.gltf");
        loadProp("pebble",      natureDir / "Pebble_Round_1.gltf");

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

        // ZONE 1: Western Industrial City Outskirts (X: -120 to -30)
        for (float x = -115.0f; x <= -30.0f; x += 12.0f) {
            placeProp("cityStreet", glm::vec3(x, 0.0f, 0.0f), 1.0f, 0.0f);
            placeProp("citySidewalk", glm::vec3(x, 0.0f, 3.2f), 1.0f, 0.0f);
        }
        placeProp("cityBldLarge", glm::vec3(-112.0f, 0.0f, -6.5f), 1.0f, 0.0f);
        placeProp("cityBldMed",   glm::vec3(-95.0f,  0.0f, -5.5f), 1.0f, 0.0f);
        placeProp("cityBldLarge", glm::vec3(-78.0f,  0.0f, -6.0f), 1.1f, 180.0f);
        placeProp("cityBldMed",   glm::vec3(-62.0f,  0.0f, -5.0f), 1.0f, 0.0f);
        placeProp("cityBldSmall", glm::vec3(-48.0f,  0.0f, -4.5f), 1.0f, 0.0f);
        placeProp("cityBldSmall", glm::vec3(-36.0f,  0.0f, -4.0f), 0.95f, 0.0f);
        for (float x = -105.0f; x <= -35.0f; x += 16.0f) {
            placeProp("crate", glm::vec3(x + 2.0f, 0.0f, 2.2f), 1.2f, 15.0f);
            placeProp("crate", glm::vec3(x + 2.8f, 0.0f, 2.0f), 1.0f, 40.0f);
            placeProp("crate", glm::vec3(x + 2.4f, 0.9f, 2.1f), 0.9f, -10.0f);
            placeProp("treeDead1", glm::vec3(x - 5.0f, 0.0f, -3.2f), 0.9f, 30.0f);
            placeProp("grassShort", glm::vec3(x - 2.0f, 0.0f, 2.8f), 0.8f, x * 7.0f);
            placeProp("pebble", glm::vec3(x + 4.0f, 0.0f, 1.2f), 1.0f, x * 13.0f);
        }

        // ZONE 2: The Medieval Village of Kaalvan (X: -28 to +28)
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

        // ZONE 3: Ancient Kaalvan Deep Forest (X: +28 to +120)
        for (float x = 28.0f; x <= 120.0f; x += 4.5f) {
            std::string tName = ((int)x % 3 == 0) ? "treeCommon1" : (((int)x % 3 == 1) ? "treeCommon2" : "treeTwisted");
            float scale = 0.85f + 0.35f * std::sin(x * 0.4f);
            float rot = (float)((int)(x * 19.0f) % 360);
            placeProp(tName, glm::vec3(x, 0.0f, -4.2f + 1.2f * std::sin(x * 0.2f)), scale, rot);
            if ((int)x % 9 == 0) placeProp("treeDead2", glm::vec3(x + 2.0f, 0.0f, -5.5f), 1.1f, rot + 45.0f);
            if ((int)x % 7 == 0) placeProp("pine", glm::vec3(x - 1.5f, 0.0f, -6.5f), 1.3f, rot + 90.0f);
        }
        for (float x = 32.0f; x <= 118.0f; x += 7.0f) {
            placeProp("rock1", glm::vec3(x, 0.0f, -0.8f + 0.6f * std::sin(x)), 0.9f, x * 15.0f);
            placeProp("bush",  glm::vec3(x + 2.5f, 0.0f, 0.2f), 1.0f, x * 30.0f);
            placeProp("mushroom", glm::vec3(x - 1.2f, 0.0f, -0.3f), 1.2f, x * 12.0f);
            placeProp("grassTall", glm::vec3(x + 1.0f, 0.0f, -0.5f), 1.0f, x * 21.0f);
        }
        for (float x = 30.0f; x <= 115.0f; x += 11.0f) {
            placeProp("treeTwisted", glm::vec3(x, 0.0f, 3.2f), 1.25f, x * 22.0f);
            placeProp("fern", glm::vec3(x + 1.5f, 0.0f, 2.5f), 1.1f, x * 17.0f);
            placeProp("fern", glm::vec3(x - 1.5f, 0.0f, 2.6f), 1.0f, x * 29.0f);
        }

        // --- 5. Characters Integration (Prefer Medieval Outfits, Fallback to Base) ---
        fs::path modularOutfitDir = assetsRoot / "Modular Character Outfits - Fantasy[Standard]" / "Exports" / "glTF (Godot-Unreal)" / "Outfits";
        fs::path maleRangerPath   = modularOutfitDir / "Male_Ranger.gltf";
        fs::path femaleRangerPath = modularOutfitDir / "Female_Ranger.gltf";
        fs::path baseMalePath     = assetsRoot / "Universal Base Characters[Standard]" / "Universal Base Characters[Standard]" / "Base Characters" / "Godot - UE" / "Superhero_Male_FullBody.gltf";
        fs::path baseFemalePath   = assetsRoot / "Universal Base Characters[Standard]" / "Universal Base Characters[Standard]" / "Base Characters" / "Godot - UE" / "Superhero_Female_FullBody.gltf";

        SkinnedModel playerMale;
        bool hasMale = playerMale.loadGLB(maleRangerPath.string());
        if (!hasMale) hasMale = playerMale.loadGLB(baseMalePath.string());
        std::printf("[Character] Male Protagonist (Ranger / Aarav): %s\n", hasMale ? "LOADED" : "FAILED");

        SkinnedModel playerFemale;
        bool hasFemale = playerFemale.loadGLB(femaleRangerPath.string());
        if (!hasFemale) hasFemale = playerFemale.loadGLB(baseFemalePath.string());
        std::printf("[Character] Female Companion / Ranger: %s\n", hasFemale ? "LOADED" : "FAILED");

        // --- 6. Monsters & Creatures Integration ---
        fs::path bestiaryDir = assetsRoot / "Bestiary - Dungeon Monsters Kit[Standard]" / "Exports" / "GLB (Godot-Unreal)";
        fs::path objDir      = assetsRoot / "OBJ";

        Model monsterImp;
        bool hasImp = monsterImp.loadGLB((bestiaryDir / "Imp.glb").string());
        std::printf("[Bestiary] Imp Creature: %s\n", hasImp ? "LOADED" : "FAILED");

        Model monsterPuglin;
        bool hasPuglin = monsterPuglin.loadGLB((bestiaryDir / "Puglin.glb").string());
        std::printf("[Bestiary] Puglin Creature: %s\n", hasPuglin ? "LOADED" : "FAILED");

        Model monsterSkeleton;
        bool hasSkeleton = monsterSkeleton.loadOBJ((objDir / "Skeleton.obj").string());
        std::printf("[Bestiary] Graveyard Skeleton: %s\n", hasSkeleton ? "LOADED" : "FAILED");

        Model monsterDragon;
        bool hasDragon = monsterDragon.loadOBJ((objDir / "Dragon.obj").string());
        std::printf("[Bestiary] Shrine Dragon: %s\n", hasDragon ? "LOADED" : "FAILED");

        // --- 7. Weapons Integration ---
        fs::path shotgunObj = assetsRoot / "PSXShotgunPack[FIXED]" / "Files" / "DoubleBarrelShotgun" / "DoubleBarrelShotgun.obj";
        fs::path shotgunTex = assetsRoot / "PSXShotgunPack[FIXED]" / "Files" / "DoubleBarrelShotgun" / "Textures" / "DBShotgunText.png";
        Model weaponShotgun;
        bool hasShotgun = weaponShotgun.loadOBJ(shotgunObj.string(), shotgunTex.string());
        std::printf("[Arsenal] Double Barrel Shotgun: %s\n", hasShotgun ? "LOADED" : "FAILED");

        Model weaponClaymore;
        bool hasClaymore = weaponClaymore.loadOBJ((objDir / "Claymore.obj").string());
        std::printf("[Arsenal] Medieval Claymore: %s\n", hasClaymore ? "LOADED" : "FAILED");

        Model weaponAxe;
        bool hasAxe = weaponAxe.loadOBJ((objDir / "Axe.obj").string());

        Model weaponShield;
        bool hasShield = weaponShield.loadOBJ((objDir / "Shield_Round.obj").string());
        std::printf("[Arsenal] War Axe & Shield: %s\n", (hasAxe && hasShield) ? "LOADED" : "FAILED");

        // --- 8. Animation System ---
        int activeCharIdx = 0; // 0 = Male, 1 = Female
        bool lastCKeyState = false;

        fs::path ual1Path = assetsRoot / "Universal Animation Library[Standard]" / "Universal Animation Library[Standard]" / "Unreal-Godot" / "UAL1_Standard.glb";
        fs::path ual2Path = assetsRoot / "Universal Animation Library 2[Standard]" / "Universal Animation Library 2[Standard]" / "Unreal-Godot" / "UAL2_Standard.glb";

        AnimationLibrary animLib;
        bool okAnim1 = animLib.loadGLBAnimations(ual1Path.string());
        bool okAnim2 = animLib.loadGLBAnimations(ual2Path.string());
        std::printf("[Anim] Universal Animations: %s\n", (okAnim1 || okAnim2) ? "LOADED" : "FAILED");

        Animator animator;
        animator.init(animLib);

        // --- 9. Audio Engine Initialized with Spooky Ambience BGM ---
        AudioEngine& audio = AudioEngine::instance();
        audio.init();
        audio.playBGM("Horror SFX Free/Stingers and Spooky Triggers/Spooky Ambience.wav", true);

        CharacterPhysics playerPhysics;
        PlayerMeters meters;

        // Ground and interactive village/city platforms
        std::vector<ObstaclePlatform> platforms;
        platforms.push_back({AABB{glm::vec3(-150.0f, -1.0f, -10.0f), glm::vec3(150.0f, 0.0f, 10.0f)}, SurfaceType::Normal, false, 1});
        platforms.push_back({AABB{glm::vec3(-17.5f, 0.0f, -2.0f), glm::vec3(-15.5f, 0.85f, -0.4f)}, SurfaceType::OneWay, false, 1});
        platforms.push_back({AABB{glm::vec3(-23.0f, 0.0f, -2.5f), glm::vec3(-21.0f, 0.95f, -0.6f)}, SurfaceType::OneWay, false, 1});
        platforms.push_back({AABB{glm::vec3(14.0f, 0.0f, -2.0f), glm::vec3(16.0f, 0.85f, -0.4f)}, SurfaceType::OneWay, false, 1});

        float camX = 0.0f;
        double lastTime = glfwGetTime();

        // Combat & Weapon State
        int equippedWeapon = 0; // 0: Lantern, 1: Claymore, 2: Shotgun, 3: Axe & Shield
        bool lastFKeyState = false;
        bool lastXKeyState = false;
        bool lastLKeyState = false;
        bool flashlightOn = true;
        float muzzleFlashTimer = 0.0f;

        std::printf("\n======================================================\n");
        std::printf("  The Seventh Night — Narrative Survival Horror Engine\n");
        std::printf("======================================================\n");
        std::printf("Controls:\n");
        std::printf("  A / D (Left/Right) : Move Left / Right (Planar 2.5D)\n");
        std::printf("  W / S (Up/Down)    : Move Depth (Z-Axis path traversal)\n");
        std::printf("  Space / Shift      : Jump / Sprint (Footstep SFX)\n");
        std::printf("  F                  : Cycle Weapon (Lantern -> Claymore -> Shotgun -> Axe)\n");
        std::printf("  X / Left Mouse     : Attack / Fire (Swing woosh & Shotgun blast)\n");
        std::printf("  L                  : Toggle Lantern / Flashlight (Click SFX)\n");
        std::printf("  C                  : Switch Playable Character (Ranger / Companion)\n");
        std::printf("  Q / E              : Lower / Raise Will (Light intensity)\n");
        std::printf("  R                  : Mauli Red Thread Supernatural Sense\n");
        std::printf("  Esc                : Quit Game\n");
        std::printf("======================================================\n\n");
        std::fflush(stdout);

        runPhysicsPhase0Test();

        while (!glfwWindowShouldClose(window)) {
            double now = glfwGetTime();
            float dt = (float)std::min(now - lastTime, 0.05);
            lastTime = now;

            if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;

            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);

            // Toggle active character with 'C' key
            bool cKeyNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            if (cKeyNow && !lastCKeyState) {
                if (hasMale && hasFemale) {
                    activeCharIdx = 1 - activeCharIdx;
                    std::printf("[Character] Active protagonist switched to: %s\n", activeCharIdx == 0 ? "Male Ranger" : "Female Ranger");
                }
            }
            lastCKeyState = cKeyNow;

            // Toggle equipped weapon with 'F' key
            bool fKeyNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
            if (fKeyNow && !lastFKeyState) {
                equippedWeapon = (equippedWeapon + 1) % 4;
                const char* wNames[] = {"Lantern / Unarmed", "Claymore", "Double Barrel Shotgun", "War Axe & Round Shield"};
                std::printf("[Arsenal] Equipped: %s\n", wNames[equippedWeapon]);
                audio.playWeaponSwing();
            }
            lastFKeyState = fKeyNow;

            // Combat Attack / Fire Action with 'X' key or Left Mouse Button
            bool xKeyNow = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (xKeyNow && !lastXKeyState) {
                if (equippedWeapon == 1) {
                    audio.playWeaponSwing();
                    std::printf("[Combat] Cleave attack with Claymore!\n");
                } else if (equippedWeapon == 2) {
                    audio.playMonsterRoar();
                    audio.playStinger();
                    muzzleFlashTimer = 0.20f;
                    std::printf("[Combat] BOOM! Double-Barrel Shotgun discharged!\n");
                } else if (equippedWeapon == 3) {
                    audio.playWeaponSwing();
                    std::printf("[Combat] Heavy strike with War Axe!\n");
                } else {
                    audio.playWeaponSwing();
                    std::printf("[Combat] Swung lantern defensively!\n");
                }
            }
            lastXKeyState = xKeyNow;

            // Flashlight / Lantern Toggle with 'L' key
            bool lKeyNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
            if (lKeyNow && !lastLKeyState) {
                flashlightOn = !flashlightOn;
                audio.playFlashlight(flashlightOn);
            }
            lastLKeyState = lKeyNow;

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

            // Footstep audio triggered during movement
            if (moveLeft || moveRight || moveForward || moveBackward) {
                audio.playFootstep(sprint);
            }

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) meters.will = std::max(0.0f, meters.will - 0.6f * dt);
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) meters.will = std::min(1.0f, meters.will + 0.6f * dt);

            // Physics & Meters Update
            playerPhysics.update(dt, platforms, moveLeft, moveRight, sprint, jumpReq, dropReq, moveForward, moveBackward);
            meters.update(dt, false, sprint && (moveLeft || moveRight || moveForward || moveBackward), steadyBreath, redThread);

            // Positional Audio Check against nearest Monster
            std::vector<glm::vec3> monsterPositions;
            if (hasImp) monsterPositions.push_back(glm::vec3(52.0f, 0.0f, -1.2f));
            if (hasPuglin) monsterPositions.push_back(glm::vec3(88.0f, 0.0f, -0.8f));
            if (hasSkeleton) monsterPositions.push_back(glm::vec3(132.0f, 0.0f, -1.0f));
            if (hasDragon) monsterPositions.push_back(glm::vec3(175.0f, 2.5f, -6.0f));

            glm::vec3 nearestMonsterPos(9999.0f);
            float nearestDist = 9999.0f;
            for (const auto& mp : monsterPositions) {
                float d = glm::distance(glm::vec2(playerPhysics.position.x, playerPhysics.position.z), glm::vec2(mp.x, mp.z));
                if (d < nearestDist) {
                    nearestDist = d;
                    nearestMonsterPos = mp;
                }
            }
            audio.update(dt, playerPhysics.position, nearestMonsterPos);

            // Determine active and companion characters
            SkinnedModel& activePlayer = (activeCharIdx == 0 && hasMale) ? playerMale : ((hasFemale) ? playerFemale : playerMale);
            bool hasSkinnedPlayer = (activeCharIdx == 0) ? hasMale : hasFemale;

            SkinnedModel* companionChar = nullptr;
            if (activeCharIdx == 0 && hasFemale) {
                companionChar = &playerFemale;
            } else if (activeCharIdx == 1 && hasMale) {
                companionChar = &playerMale;
            }

            // Animation Update
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

            // Smooth Cinematic Camera Tracking
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

            glUseProgram(program);
            glUniformMatrix4fv(glGetUniformLocation(program, "uView"), 1, GL_FALSE, &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "uProj"), 1, GL_FALSE, &proj[0][0]);
            glUniform3fv(glGetUniformLocation(program, "uCamPos"), 1, &camPos[0]);

            glm::vec3 lanternPos = playerPhysics.position + glm::vec3(0.0f, 1.2f, 0.4f);
            glUniform3fv(glGetUniformLocation(program, "uLightPos"), 1, &lanternPos[0]);

            float flicker = 0.92f + 0.08f * std::sin((float)now * 17.0f) * std::sin((float)now * 5.3f);
            float lightWill = (flashlightOn ? meters.will : 0.05f) * flicker + (muzzleFlashTimer > 0.0f ? 4.5f : 0.0f);
            glUniform1f(glGetUniformLocation(program, "uWill"), lightWill);
            glUniform1f(glGetUniformLocation(program, "uRedGlow"), meters.redThreadGlow);

            // 1. Render Atmospheric Parallax Visual Novel Backdrops (Painted Night Horizons at Z = -22.0)
            glUniform1i(glGetUniformLocation(program, "uIsBackdrop"), 1);
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);

            // Zone 1 City Horizon
            glm::mat4 bgCityMat = glm::translate(glm::mat4(1.0f), glm::vec3(-72.0f, 13.0f, -22.0f));
            bgCity.draw(program, bgCityMat);

            // Zone 2 Village Square Horizon
            glm::mat4 bgVillageMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 13.0f, -22.0f));
            bgVillage.draw(program, bgVillageMat);

            // Zone 3 Deep Forest Horizon
            glm::mat4 bgForestMat = glm::translate(glm::mat4(1.0f), glm::vec3(74.0f, 13.0f, -22.0f));
            bgForest.draw(program, bgForestMat);

            // Zone 4 Graveyard & Ruin Horizon
            glm::mat4 bgGraveyardMat = glm::translate(glm::mat4(1.0f), glm::vec3(145.0f, 13.0f, -22.0f));
            bgGraveyard.draw(program, bgGraveyardMat);

            glUniform1i(glGetUniformLocation(program, "uIsBackdrop"), 0);

            // 2. Draw Ground Plane
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
            ground.draw(program, glm::mat4(1.0f));

            // 3. Draw World Props (City buildings, cottages, wagons, trees, rocks)
            for (const auto& prop : worldProps) {
                if (prop.modelIdx < propTemplates.size()) {
                    glm::mat4 m = glm::translate(glm::mat4(1.0f), prop.pos);
                    m = glm::rotate(m, glm::radians(prop.rotationY), glm::vec3(0, 1, 0));
                    m = glm::scale(m, glm::vec3(prop.scale));
                    propTemplates[prop.modelIdx].draw(program, m);
                }
            }

            // 4. Draw Bestiary Monsters & Fantasy Creatures
            if (hasImp) {
                glm::mat4 impMat = glm::translate(glm::mat4(1.0f), glm::vec3(52.0f, 0.0f, -1.2f));
                impMat = glm::rotate(impMat, glm::radians(35.0f + 8.0f * std::sin((float)now * 2.5f)), glm::vec3(0, 1, 0));
                monsterImp.draw(program, impMat);
            }
            if (hasPuglin) {
                glm::mat4 pugMat = glm::translate(glm::mat4(1.0f), glm::vec3(88.0f, 0.0f, -0.8f));
                pugMat = glm::rotate(pugMat, glm::radians(-35.0f), glm::vec3(0, 1, 0));
                monsterPuglin.draw(program, pugMat);
            }
            if (hasSkeleton) {
                glm::mat4 skelMat1 = glm::translate(glm::mat4(1.0f), glm::vec3(132.0f, 0.0f, -1.0f));
                skelMat1 = glm::scale(skelMat1, glm::vec3(1.2f));
                skelMat1 = glm::rotate(skelMat1, glm::radians(55.0f), glm::vec3(0, 1, 0));
                monsterSkeleton.draw(program, skelMat1);

                glm::mat4 skelMat2 = glm::translate(glm::mat4(1.0f), glm::vec3(148.0f, 0.0f, 1.1f));
                skelMat2 = glm::scale(skelMat2, glm::vec3(1.15f));
                skelMat2 = glm::rotate(skelMat2, glm::radians(-125.0f), glm::vec3(0, 1, 0));
                monsterSkeleton.draw(program, skelMat2);
            }
            if (hasDragon) {
                glm::mat4 dragMat = glm::translate(glm::mat4(1.0f), glm::vec3(175.0f, 2.5f, -6.0f));
                dragMat = glm::scale(dragMat, glm::vec3(2.4f));
                dragMat = glm::rotate(dragMat, glm::radians(210.0f), glm::vec3(0, 1, 0));
                monsterDragon.draw(program, dragMat);
            }

            // 5. Draw Village Companion Character (Kaalvan Village Shrine at X = 6.0)
            if (companionChar) {
                glm::mat4 compMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -0.6f));
                compMatrix = glm::rotate(compMatrix, glm::radians(180.0f), glm::vec3(0, 1, 0));
                companionChar->draw(program, compMatrix);
            }

            // 6. Draw Active Player Character (Skinned mesh or fallback)
            glm::mat4 playerModelMatrix = glm::translate(glm::mat4(1.0f), playerPhysics.position);
            playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(animator.getFacingAngleDegrees()), glm::vec3(0, 1, 0));
            if (hasSkinnedPlayer) {
                activePlayer.draw(program, playerModelMatrix);
            } else {
                glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
                playerFallback.draw(program, playerModelMatrix);
            }

            // 7. Draw Equipped Weapon attached to Character
            glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 0);
            if (equippedWeapon == 1 && hasClaymore) {
                glm::mat4 wMat = playerModelMatrix;
                wMat = glm::translate(wMat, glm::vec3(0.3f, 0.85f, 0.25f));
                wMat = glm::rotate(wMat, glm::radians(20.0f), glm::vec3(0, 0, 1));
                wMat = glm::scale(wMat, glm::vec3(0.65f));
                weaponClaymore.draw(program, wMat);
            } else if (equippedWeapon == 2 && hasShotgun) {
                glm::mat4 wMat = playerModelMatrix;
                wMat = glm::translate(wMat, glm::vec3(0.25f, 0.8f, 0.35f));
                wMat = glm::rotate(wMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
                wMat = glm::scale(wMat, glm::vec3(0.40f));
                weaponShotgun.draw(program, wMat);
            } else if (equippedWeapon == 3 && hasAxe) {
                glm::mat4 axeMat = playerModelMatrix;
                axeMat = glm::translate(axeMat, glm::vec3(0.35f, 0.8f, 0.25f));
                axeMat = glm::scale(axeMat, glm::vec3(0.7f));
                weaponAxe.draw(program, axeMat);

                if (hasShield) {
                    glm::mat4 shMat = playerModelMatrix;
                    shMat = glm::translate(shMat, glm::vec3(-0.35f, 0.8f, 0.15f));
                    shMat = glm::rotate(shMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
                    shMat = glm::scale(shMat, glm::vec3(0.85f));
                    weaponShield.draw(program, shMat);
                }
            }

            glfwSwapBuffers(window);
        }
    }

    AudioEngine::instance().shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
