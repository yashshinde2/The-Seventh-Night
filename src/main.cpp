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
#include "scene/Scene.h"
#include "render/SceneRenderer.h"

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    bool testMode = false;
    std::string customScenePath = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" || arg == "--headless-test") {
            testMode = true;
        } else if (arg.find(".scene") != std::string::npos) {
            customScenePath = arg;
        }
    }

    if (!glfwInit()) {
        std::fprintf(stderr, "Could not start GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (testMode) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Offscreen for automated testing
    }

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

    // --- Core Subsystems Initialization ---
    SceneRenderer renderer;
    if (!renderer.init()) {
        std::fprintf(stderr, "[Error] Failed to initialize SceneRenderer\n");
        return 1;
    }

    fs::path assetsRoot = AssetManager::findAssetsRoot(argv[0]);
    AssetManager::printDiscoveredAssets(assetsRoot);

    // --- Phase 3: Scene Loading from Scene File ---
    fs::path sceneFile = customScenePath.empty()
        ? (assetsRoot / "scenes" / "kaalvan_forest.scene")
        : fs::path(customScenePath);

    Scene currentScene;
    if (!currentScene.loadFromFile(sceneFile.string(), assetsRoot)) {
        std::fprintf(stderr, "[Error] Failed to load level scene from %s\n", sceneFile.string().c_str());
        return 1;
    }
    if (!currentScene.validate()) {
        std::fprintf(stderr, "[Warning] Scene validation found warnings\n");
    }

    // Environment Ground Plane
    Model ground = Model::groundQuad(300.0f, glm::vec4(0.18f, 0.28f, 0.18f, 1.0f));
    Model playerFallback = Model::box(glm::vec3(0.6f, 1.7f, 0.4f), glm::vec4(0.25f, 0.45f, 0.85f, 1.0f));

    // --- Characters Integration ---
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

    // --- Monsters & Creatures Integration ---
    fs::path bestiaryDir = assetsRoot / "Bestiary - Dungeon Monsters Kit[Standard]" / "Exports" / "GLB (Godot-Unreal)";
    fs::path objDir      = assetsRoot / "OBJ";

    Model monsterImp;
    bool hasImp = monsterImp.loadGLB((bestiaryDir / "Imp.glb").string());

    Model monsterPuglin;
    bool hasPuglin = monsterPuglin.loadGLB((bestiaryDir / "Puglin.glb").string());

    Model monsterSkeleton;
    bool hasSkeleton = monsterSkeleton.loadOBJ((objDir / "Skeleton.obj").string());

    Model monsterDragon;
    bool hasDragon = monsterDragon.loadOBJ((objDir / "Dragon.obj").string());

    std::map<std::string, Model*> creatureModelMap;
    if (hasImp) creatureModelMap["Imp"] = &monsterImp;
    if (hasPuglin) creatureModelMap["Puglin"] = &monsterPuglin;
    if (hasSkeleton) creatureModelMap["Skeleton"] = &monsterSkeleton;
    if (hasDragon) creatureModelMap["Dragon"] = &monsterDragon;
    std::printf("[Bestiary] Creatures registered: Imp (%d), Puglin (%d), Skeleton (%d), Dragon (%d)\n",
                hasImp, hasPuglin, hasSkeleton, hasDragon);

    // --- Weapons Integration ---
    fs::path shotgunObj = assetsRoot / "PSXShotgunPack[FIXED]" / "Files" / "DoubleBarrelShotgun" / "DoubleBarrelShotgun.obj";
    fs::path shotgunTex = assetsRoot / "PSXShotgunPack[FIXED]" / "Files" / "DoubleBarrelShotgun" / "Textures" / "DBShotgunText.png";
    Model weaponShotgun;
    bool hasShotgun = weaponShotgun.loadOBJ(shotgunObj.string(), shotgunTex.string());

    Model weaponClaymore;
    bool hasClaymore = weaponClaymore.loadOBJ((objDir / "Claymore.obj").string());

    Model weaponAxe;
    bool hasAxe = weaponAxe.loadOBJ((objDir / "Axe.obj").string());

    Model weaponShield;
    bool hasShield = weaponShield.loadOBJ((objDir / "Shield_Round.obj").string());

    // --- Animation System ---
    int activeCharIdx = 0; // 0 = Male (Aarav), 1 = Female
    bool lastCKeyState = false;

    fs::path ual1Path = assetsRoot / "Universal Animation Library[Standard]" / "Universal Animation Library[Standard]" / "Unreal-Godot" / "UAL1_Standard.glb";
    fs::path ual2Path = assetsRoot / "Universal Animation Library 2[Standard]" / "Universal Animation Library 2[Standard]" / "Unreal-Godot" / "UAL2_Standard.glb";

    AnimationLibrary animLib;
    bool okAnim1 = animLib.loadGLBAnimations(ual1Path.string());
    bool okAnim2 = animLib.loadGLBAnimations(ual2Path.string());
    std::printf("[Anim] Universal Animations: %s\n", (okAnim1 || okAnim2) ? "LOADED" : "FAILED");

    Animator animator;
    animator.init(animLib);

    // --- Audio Engine ---
    AudioEngine& audio = AudioEngine::instance();
    audio.init();
    audio.playBGM("Horror SFX Free/Stingers and Spooky Triggers/Spooky Ambience.wav", true);

    // --- Physics Engine & Player Initialization ---
    CharacterPhysics playerPhysics;
    playerPhysics.position = currentScene.atmosphere.playerSpawn;
    playerPhysics.targetLane = currentScene.atmosphere.playerSpawnLane;

    PlayerMeters meters;

    float camX = playerPhysics.position.x;
    double lastTime = glfwGetTime();

    int equippedWeapon = 0; // 0: Lantern, 1: Claymore, 2: Shotgun, 3: Axe & Shield
    bool lastFKeyState = false;
    bool lastXKeyState = false;
    bool lastLKeyState = false;
    bool flashlightOn = true;
    float muzzleFlashTimer = 0.0f;

    std::printf("\n======================================================\n");
    std::printf("  The Seventh Night — Narrative Survival Horror Engine\n");
    std::printf("  Scene Loaded: %s\n", currentScene.atmosphere.name.c_str());
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

    int testFrameCount = 0;
    const int kMaxTestFrames = 120;

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

        // Movement & Inputs
        bool moveLeft = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
        bool moveRight = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
        bool moveForward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
        bool moveBackward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
        bool sprint = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        bool jumpReq = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        bool dropReq = moveBackward && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

        // Automated test stimulus
        if (testMode) {
            if (testFrameCount < 40) moveRight = true;
            else if (testFrameCount < 80) { moveRight = true; sprint = true; }
            else if (testFrameCount < 100) jumpReq = true;
        }

        bool steadyBreath = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && !moveLeft && !moveRight && !moveForward && !moveBackward;
        bool redThread = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;

        // Footstep audio
        if (moveLeft || moveRight || moveForward || moveBackward) {
            audio.playFootstep(sprint);
        }

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) meters.will = std::max(0.0f, meters.will - 0.6f * dt);
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) meters.will = std::min(1.0f, meters.will + 0.6f * dt);

        // Physics & Meters Update using Scene Platforms
        playerPhysics.update(dt, currentScene.platforms, moveLeft, moveRight, sprint, jumpReq, dropReq, moveForward, moveBackward);
        meters.update(dt, false, sprint && (moveLeft || moveRight || moveForward || moveBackward), steadyBreath, redThread);

        // Positional Audio Check against nearest Monster
        glm::vec3 nearestMonsterPos(9999.0f);
        float nearestDist = 9999.0f;
        for (const auto& c : currentScene.creatures) {
            float d = glm::distance(glm::vec2(playerPhysics.position.x, playerPhysics.position.z), glm::vec2(c.pos.x, c.pos.z));
            if (d < nearestDist) {
                nearestDist = d;
                nearestMonsterPos = c.pos;
            }
        }
        audio.update(dt, playerPhysics.position, nearestMonsterPos);

        // Active and companion characters
        SkinnedModel& activePlayer = (activeCharIdx == 0 && hasMale) ? playerMale : ((hasFemale) ? playerFemale : playerMale);
        bool hasSkinnedPlayer = (activeCharIdx == 0) ? hasMale : hasFemale;

        SkinnedModel* companionChar = nullptr;
        if (activeCharIdx == 0 && hasFemale) companionChar = &playerFemale;
        else if (activeCharIdx == 1 && hasMale) companionChar = &playerMale;

        // Animation Update
        if (hasSkinnedPlayer) {
            animator.update(dt, playerPhysics, activePlayer);
        }

        if (companionChar) {
            const AnimationClip* companionClip = animLib.findClipAny({"Idle_Talking_Loop", "Sitting_Idle_Loop", "Idle_Loop", "Idle"});
            if (companionClip) {
                companionChar->updateSkeleton(companionClip, (float)now, true);
            }
        }

        // Camera Tracking
        camX += (playerPhysics.position.x - camX) * std::min(1.0f, 4.0f * dt);
        glm::vec3 camPos(camX, 2.6f, 10.0f);
        glm::vec3 target(camX, 1.3f, playerPhysics.position.z * 0.35f);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);

        glm::vec3 lanternPos = playerPhysics.position + glm::vec3(0.0f, 1.2f, 0.4f);
        float flicker = 0.92f + 0.08f * std::sin((float)now * 17.0f) * std::sin((float)now * 5.3f);
        float lightWill = (flashlightOn ? meters.will : 0.05f) * flicker;

        // --- Render Pass using SceneRenderer ---
        renderer.beginFrame(fbw, fbh, camPos, target, currentScene.atmosphere, lanternPos, lightWill, meters.redThreadGlow, muzzleFlashTimer);

        // 1. Panoramic Backdrops
        renderer.drawBackdrops(currentScene.backdrops);

        // 2. Ground Plane
        renderer.drawGround(ground);

        // 3. Scene Props
        renderer.drawSceneProps(currentScene);

        // 4. Creatures
        renderer.drawCreatures(creatureModelMap, currentScene.creatures, (float)now);

        // 5. Companion Character
        if (companionChar) {
            glm::mat4 compMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(6.0f, 0.0f, -0.6f));
            compMatrix = glm::rotate(compMatrix, glm::radians(180.0f), glm::vec3(0, 1, 0));
            renderer.drawSkinnedCharacter(*companionChar, compMatrix);
        }

        // 6. Player Character
        glm::mat4 playerModelMatrix = glm::translate(glm::mat4(1.0f), playerPhysics.position);
        playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(animator.getFacingAngleDegrees()), glm::vec3(0, 1, 0));
        if (hasSkinnedPlayer) {
            renderer.drawSkinnedCharacter(activePlayer, playerModelMatrix);
        } else {
            renderer.drawStaticModel(playerFallback, playerModelMatrix);
        }

        // 7. Weapons
        if (equippedWeapon == 1 && hasClaymore) {
            glm::mat4 wMat = playerModelMatrix;
            wMat = glm::translate(wMat, glm::vec3(0.3f, 0.85f, 0.25f));
            wMat = glm::rotate(wMat, glm::radians(20.0f), glm::vec3(0, 0, 1));
            wMat = glm::scale(wMat, glm::vec3(0.65f));
            renderer.drawStaticModel(weaponClaymore, wMat);
        } else if (equippedWeapon == 2 && hasShotgun) {
            glm::mat4 wMat = playerModelMatrix;
            wMat = glm::translate(wMat, glm::vec3(0.25f, 0.8f, 0.35f));
            wMat = glm::rotate(wMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
            wMat = glm::scale(wMat, glm::vec3(0.40f));
            renderer.drawStaticModel(weaponShotgun, wMat);
        } else if (equippedWeapon == 3 && hasAxe) {
            glm::mat4 axeMat = playerModelMatrix;
            axeMat = glm::translate(axeMat, glm::vec3(0.35f, 0.8f, 0.25f));
            axeMat = glm::scale(axeMat, glm::vec3(0.7f));
            renderer.drawStaticModel(weaponAxe, axeMat);

            if (hasShield) {
                glm::mat4 shMat = playerModelMatrix;
                shMat = glm::translate(shMat, glm::vec3(-0.35f, 0.8f, 0.15f));
                shMat = glm::rotate(shMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
                shMat = glm::scale(shMat, glm::vec3(0.85f));
                renderer.drawStaticModel(weaponShield, shMat);
            }
        }

        glfwSwapBuffers(window);

        if (testMode) {
            testFrameCount++;
            if (testFrameCount >= kMaxTestFrames) {
                std::printf("[TEST MODE] Completed %d test frames successfully! Aarav position X: %.2f\n",
                            testFrameCount, playerPhysics.position.x);
                break;
            }
        }
    }

    renderer.shutdown();
    AudioEngine::instance().shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::printf("[Engine] Clean shutdown completed successfully.\n");
    return 0;
}
