#pragma once

#include <glm/glm.hpp>
#include <string>
#include "../physics/PhysicsEngine.h"
#include "../game/GameMeters.h"
#include "../audio/AudioEngine.h"
#include "../render/HudRenderer.h"

enum class Day4Phase {
    EnteringGrove,
    CollapseUnderHollowTree,
    ChoiceDecision,
    TheEmberAppears,
    TheTurningPoint,
    Day4Completed
};

class Day4Narrative {
public:
    Day4Phase phase = Day4Phase::EnteringGrove;
    float phaseTimer = 0.0f;
    float worldDesaturation = 0.0f; // 0.0 = color, 1.0 = ash grey
    bool isActive = false;

    // The Ember Character State
    glm::vec3 emberPos{27.0f, 0.0f, -0.6f};
    bool emberVisible = false;
    float emberScale = 0.55f;

    std::string dialogueSpeaker = "";
    std::string dialogueLine = "";
    glm::vec4 dialogueSpeakerColor{1.0f, 0.78f, 0.35f, 1.0f};

    std::string protagonistTitle = "AARAV";
    std::string currentDayBanner = "ACT III - DAY 4: THE GREY GROVE (THE TURNING POINT)";

    void init();
    void update(float dt, CharacterPhysics& playerPhysics, PlayerMeters& meters,
                AudioEngine& audio, bool inputRest, bool inputHoldOn);
    void renderUI(HudRenderer& hud, int screenW, int screenH);
};
