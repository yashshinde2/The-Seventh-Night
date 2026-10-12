#pragma once

#include <algorithm>

struct PlayerMeters {
    float will = 1.0f;       // 0.0 to 1.0 (Sanity / Light intensity / Hallucination threshold)
    float body = 1.0f;       // 0.0 to 1.0 (Health)
    float stamina = 1.0f;    // 0.0 to 1.0 (Movement energy)
    float hope = 0.5f;       // 0.0 to 1.0 (Narrative Ending determinant: <0.4 Bad, 0.4-0.8 Neutral, >0.8 True)
    float lanternOil = 1.0f; // 0.0 to 1.0 (Oil fuel)

    float redThreadGlow = 0.0f; // 0.0 to 1.0 (Mauli compass visual intensity)

    bool isExhausted = false;

    bool isDisoriented() const {
        return will < 0.25f;
    }

    void restoreFromMemory(float willBonus = 0.35f, float hopeBonus = 0.15f) {
        will = std::min(1.0f, will + willBonus);
        hope = std::min(1.0f, hope + hopeBonus);
    }

    void replenishOil(float amount = 0.4f) {
        lanternOil = std::min(1.0f, lanternOil + amount);
    }

    void update(float dt, bool inDarkness, bool isSprinting, bool isSteadyBreathing, bool usingRedThread) {
        // Milestone 6 Will Rules:
        // 1. Darkness drains Will steadily
        if (inDarkness) {
            will = std::max(0.0f, will - 0.08f * dt);
        } else {
            // Warm lantern / memories restore Will gradually
            if (lanternOil > 0.0f) {
                will = std::min(1.0f, will + 0.035f * dt);
                lanternOil = std::max(0.0f, lanternOil - 0.005f * dt);
            }
        }

        // 2. Steady Breathing (holding Ctrl while stationary) restores Will actively
        if (isSteadyBreathing) {
            will = std::min(1.0f, will + 0.18f * dt);
            stamina = std::max(0.0f, stamina - 0.12f * dt);
        }

        // 3. Stamina drain during sprint & natural recovery
        if (isSprinting) {
            stamina = std::max(0.0f, stamina - 0.28f * dt);
            if (stamina <= 0.0f) isExhausted = true;
        } else {
            stamina = std::min(1.0f, stamina + 0.22f * dt);
            if (stamina > 0.2f) isExhausted = false;
        }

        // 4. Red Thread supernatural focus
        if (usingRedThread) {
            redThreadGlow = std::min(1.0f, redThreadGlow + 3.0f * dt);
        } else {
            redThreadGlow = std::max(0.0f, redThreadGlow - 2.0f * dt);
        }
    }
};
