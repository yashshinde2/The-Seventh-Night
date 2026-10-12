#include "Day4Narrative.h"
#include <algorithm>
#include <cstdio>
#include <cmath>

void Day4Narrative::init() {
    phase = Day4Phase::EnteringGrove;
    phaseTimer = 0.0f;
    worldDesaturation = 0.85f;
    isActive = true;
    emberVisible = false;
    emberPos = glm::vec3(26.0f, 0.0f, -0.8f);
    protagonistTitle = "AARAV (EXHAUSTED)";
    currentDayBanner = "ACT III - DAY 4: THE GREY GROVE (THE TURNING POINT)";
    dialogueSpeaker = "Aarav";
    dialogueLine = "The trees... they look like ash. My feet feel like lead.";
    dialogueSpeakerColor = glm::vec4(0.8f, 0.85f, 0.95f, 1.0f);
    std::printf("[Day 4 Vertical Slice] The Grey Grove initiated. Desaturation active.\n");
}

void Day4Narrative::update(float dt, CharacterPhysics& playerPhysics, PlayerMeters& meters,
                           AudioEngine& audio, bool inputRest, bool inputHoldOn) {
    if (!isActive) return;

    phaseTimer += dt;

    switch (phase) {
        case Day4Phase::EnteringGrove: {
            // Forced slow walk: speed capped at 1.8 units/s, exhaustion
            playerPhysics.moveSpeed = 1.9f;
            playerPhysics.sprintMultiplier = 1.0f;

            // Desaturation deepens into total ash-grey
            worldDesaturation = std::min(1.0f, worldDesaturation + 0.06f * dt);

            // Inevitable Will drain in Grey Grove
            meters.will = std::max(0.12f, meters.will - 0.03f * dt);

            if (phaseTimer > 4.0f && phaseTimer <= 8.0f) {
                dialogueSpeaker = "The Hollow's Whisper";
                dialogueLine = "Nobody is coming... You tried. Stop walking.";
                dialogueSpeakerColor = glm::vec4(0.55f, 0.25f, 0.25f, 1.0f);
            } else if (phaseTimer > 8.0f) {
                dialogueSpeaker = "Aarav";
                dialogueLine = "I can't... I can't keep going. There's a hollow tree ahead...";
                dialogueSpeakerColor = glm::vec4(0.8f, 0.85f, 0.95f, 1.0f);
            }

            // Approaching the Hollow Tree
            if (playerPhysics.position.x >= 21.5f || phaseTimer > 18.0f) {
                phase = Day4Phase::CollapseUnderHollowTree;
                phaseTimer = 0.0f;
                playerPhysics.velocity = glm::vec3(0.0f);
                std::printf("[Day 4] Aarav collapses under the Hollow Tree.\n");
            }
            break;
        }

        case Day4Phase::CollapseUnderHollowTree: {
            // Sitting under the tree
            playerPhysics.velocity = glm::vec3(0.0f);
            dialogueSpeaker = "Aarav";
            dialogueLine = "What's the point? Dad lost everything because of me. Nobody will notice.";
            dialogueSpeakerColor = glm::vec4(0.75f, 0.80f, 0.90f, 1.0f);

            if (phaseTimer > 3.0f) {
                phase = Day4Phase::ChoiceDecision;
                phaseTimer = 0.0f;
            }
            break;
        }

        case Day4Phase::ChoiceDecision: {
            playerPhysics.velocity = glm::vec3(0.0f);
            dialogueSpeaker = "The Hollow's Whisper";
            dialogueLine = "Close your eyes. Give up. Rest.";
            dialogueSpeakerColor = glm::vec4(0.6f, 0.2f, 0.2f, 1.0f);

            // Decision: Rest or Hold On
            if (inputHoldOn || phaseTimer > 8.0f) { // In automated/test mode or keypress, hold on!
                phase = Day4Phase::TheEmberAppears;
                phaseTimer = 0.0f;
                meters.redThreadGlow = 1.0f; // Red thread flares brightly
                audio.playStinger();
                std::printf("[Day 4] Aarav chose to HOLD ON! The Mauli Red Thread flares with warmth.\n");
            } else if (inputRest) {
                // Temporary fade into deep sleep before thread intervention
                dialogueSpeaker = "Aarav";
                dialogueLine = "I'm just so tired...";
                if (phaseTimer > 3.0f) {
                    phase = Day4Phase::TheEmberAppears;
                    phaseTimer = 0.0f;
                    meters.redThreadGlow = 1.0f;
                }
            }
            break;
        }

        case Day4Phase::TheEmberAppears: {
            playerPhysics.velocity = glm::vec3(0.0f);
            emberVisible = true;

            // The Ember walks from hollow tree (X=26.0) towards Aarav (X=22.5)
            if (emberPos.x > 22.8f) {
                emberPos.x -= 1.2f * dt;
            }

            dialogueSpeaker = "The Ember";
            dialogueLine = "\"Papa is still waiting. Are you?\"";
            dialogueSpeakerColor = glm::vec4(1.0f, 0.85f, 0.35f, 1.0f);

            if (phaseTimer > 4.5f) {
                phase = Day4Phase::TheTurningPoint;
                phaseTimer = 0.0f;
                audio.playMatchStrike();
                std::printf("[Day 4] The Ember hands Aarav the Golden Diya. The turning point has arrived!\n");
            }
            break;
        }

        case Day4Phase::TheTurningPoint: {
            // Colors return in warm wave, radiating from the diya!
            worldDesaturation = std::max(0.0f, worldDesaturation - 0.35f * dt);
            meters.will = std::min(1.0f, meters.will + 0.25f * dt);
            meters.hope = std::min(1.0f, meters.hope + 0.15f * dt);
            meters.lanternOil = 1.0f;

            protagonistTitle = "AARAV THE LAMPLIGHTER";
            dialogueSpeaker = "Aarav the Lamplighter";
            dialogueLine = "\"Okay. One more step.\"";
            dialogueSpeakerColor = glm::vec4(1.0f, 0.88f, 0.40f, 1.0f);

            if (phaseTimer > 3.5f) {
                phase = Day4Phase::Day4Completed;
                phaseTimer = 0.0f;
                playerPhysics.moveSpeed = 4.5f;
                playerPhysics.sprintMultiplier = 1.6f;
                std::printf("[Day 4 Vertical Slice Completed] Hope is rekindled! Color restored.\n");
            }
            break;
        }

        case Day4Phase::Day4Completed: {
            worldDesaturation = 0.0f;
            emberVisible = false;
            dialogueSpeaker = "Aarav the Lamplighter";
            dialogueLine = "Hope is not a feeling you wait for. It is a step you take.";
            dialogueSpeakerColor = glm::vec4(1.0f, 0.88f, 0.40f, 1.0f);
            if (phaseTimer > 5.0f) {
                dialogueLine = ""; // Clear subtitles after narrative beat finishes
            }
            break;
        }
    }
}

void Day4Narrative::renderUI(HudRenderer& hud, int screenW, int screenH) {
    if (!isActive) return;

    if (phase == Day4Phase::ChoiceDecision) {
        hud.drawChoiceBox(screenW, screenH,
                          "R", "REST — Surrender to the cold and give up",
                          "SPACE", "HOLD ON — Trust Meera's thread and remember Papa");
    }

    if (!dialogueLine.empty()) {
        hud.drawDialogueBox(screenW, screenH, dialogueSpeaker, dialogueLine, dialogueSpeakerColor);
    }
}
