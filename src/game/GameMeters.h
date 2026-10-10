#pragma once

#include <algorithm>

struct PlayerMeters {
    float will = 1.0f;       // 0.0 to 1.0 (Sanity / Light intensity)
    float body = 1.0f;       // 0.0 to 1.0 (Health)
    float stamina = 1.0f;    // 0.0 to 1.0 (Movement energy)
    float hope = 0.5f;       // 0.0 to 1.0 (Narrative Ending determinant: <0.4 Bad, 0.4-0.8 Neutral, >0.8 True)
    float lanternOil = 1.0f; // 0.0 to 1.0 (Oil fuel)

    float redThreadGlow = 0.0f; // 0.0 to 1.0 (Mauli compass visual intensity)

    bool isExhausted = false;

    void update(float dt, bool inDarkness, bool isSprinting, bool isSteadyBreathing, bool usingRedThread) {
        // Will drains in darkness, recovers during Steady Breath
        if (inDarkness) {
            will = std::max(0.0f, will - 0.05f * dt);
        }
        if (isSteadyBreathing) {
            will = std::min(1.0f, will + 0.15f * dt);
            stamina = std::max(0.0f, stamina - 0.1f * dt);
        }

        // Stamina drain & recovery
        if (isSprinting) {
            stamina = std::max(0.0f, stamina - 0.25f * dt);
            if (stamina <= 0.0f) isExhausted = true;
        } else {
            stamina = std::min(1.0f, stamina + 0.2f * dt);
            if (stamina > 0.2f) isExhausted = false;
        }

        // Red Thread glow effect
        if (usingRedThread) {
            redThreadGlow = std::min(1.0f, redThreadGlow + 3.0f * dt);
        } else {
            redThreadGlow = std::max(0.0f, redThreadGlow - 2.0f * dt);
        }
    }
};
