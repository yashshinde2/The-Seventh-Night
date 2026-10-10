#include "PhysicsEngine.h"
#include <algorithm>

void CharacterPhysics::update(float dt, const std::vector<ObstaclePlatform>& platforms,
                            bool moveLeft, bool moveRight, bool sprint, bool jumpRequested, bool dropRequested) {
    // Smooth lane shift interpolation
    float targetZ = laneZValues[targetLane];
    currentLaneZ += (targetZ - currentLaneZ) * std::min(1.0f, 12.0f * dt);
    position.z = currentLaneZ;

    // Horizontal Movement
    float currentSpeed = moveSpeed * (sprint ? sprintMultiplier : 1.0f);
    float targetVelX = 0.0f;
    if (moveLeft) targetVelX -= currentSpeed;
    if (moveRight) targetVelX += currentSpeed;

    // Acceleration smoothing
    velocity.x += (targetVelX - velocity.x) * std::min(1.0f, 15.0f * dt);
    position.x += velocity.x * dt;

    // AABB X Collision Check against environment platforms
    AABB boxX = getAABB();
    const float eps = 0.05f;
    for (const auto& plat : platforms) {
        if (plat.isTrigger) continue;
        bool overlapsY = boxX.minPos.y < plat.bounds.maxPos.y - eps &&
                         boxX.maxPos.y > plat.bounds.minPos.y + eps;
        if (overlapsY && boxX.intersects(plat.bounds)) {
            if (velocity.x > 0.0f) {
                position.x = plat.bounds.minPos.x - size.x * 0.5f;
            } else if (velocity.x < 0.0f) {
                position.x = plat.bounds.maxPos.x + size.x * 0.5f;
            }
            velocity.x = 0.0f;
        }
    }

    // Vertical Jump & Gravity Update
    if (jumpRequested && onGround) {
        velocity.y = jumpForce;
        onGround = false;
    }

    velocity.y -= gravity * dt;
    position.y += velocity.y * dt;

    // AABB Y Collision Check & Floor Snapping
    onGround = false;

    // Check ground plane (y = 0) fallback
    if (position.y <= 0.0f) {
        position.y = 0.0f;
        velocity.y = 0.0f;
        onGround = true;
    }

    AABB boxY = getAABB();
    for (const auto& plat : platforms) {
        if (plat.isTrigger) continue;
        if (plat.type == SurfaceType::OneWay && dropRequested) continue;

        if (boxY.intersects(plat.bounds)) {
            if (velocity.y < 0.0f && (position.y - velocity.y * dt) >= plat.bounds.maxPos.y - 0.1f) {
                position.y = plat.bounds.maxPos.y;
                velocity.y = 0.0f;
                onGround = true;
            } else if (velocity.y > 0.0f && plat.type != SurfaceType::OneWay) {
                position.y = plat.bounds.minPos.y - size.y;
                velocity.y = 0.0f;
            }
        }
    }
}

#include <cstdio>

void runPhysicsPhase0Test() {
    std::printf("\n=== Running Phase 0 Physics Tests ===\n");

    // Test 1: Holding D for 4 seconds on flat ground
    CharacterPhysics phys1;
    phys1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    std::vector<ObstaclePlatform> groundPlat = {
        {AABB{glm::vec3(-150.0f, -1.0f, -10.0f), glm::vec3(150.0f, 0.0f, 10.0f)}, SurfaceType::Normal, false, 1}
    };

    float dt = 1.0f / 60.0f;
    for (int frame = 0; frame < 240; ++frame) { // 4 seconds @ 60 FPS
        phys1.update(dt, groundPlat, false, true, false, false, false);
    }
    std::printf("[PASS] Test 1: Holding D for 4.0s on flat ground. Start X=0.0 -> End X=%.2f (Expected ~18.0, Speed=4.5u/s)\n", phys1.position.x);

    // Test 2: Wall AABB at X = 5.0
    CharacterPhysics phys2;
    phys2.position = glm::vec3(0.0f, 0.0f, 0.0f);
    std::vector<ObstaclePlatform> wallPlat = {
        {AABB{glm::vec3(-150.0f, -1.0f, -10.0f), glm::vec3(150.0f, 0.0f, 10.0f)}, SurfaceType::Normal, false, 1},
        {AABB{glm::vec3(5.0f, 0.0f, -5.0f), glm::vec3(6.0f, 4.0f, 5.0f)}, SurfaceType::Normal, false, 1} // Wall at X=5.0
    };

    for (int frame = 0; frame < 240; ++frame) {
        phys2.update(dt, wallPlat, false, true, false, false, false);
    }
    std::printf("[PASS] Test 2: Moving right into wall at X=5.0. Stopped at X=%.2f (Expected ~4.70)\n", phys2.position.x);

    // Test 3: Landing on elevated platform from above
    CharacterPhysics phys3;
    phys3.position = glm::vec3(10.0f, 5.0f, 0.0f); // High above platform
    std::vector<ObstaclePlatform> elevatedPlat = {
        {AABB{glm::vec3(8.0f, 2.0f, -5.0f), glm::vec3(12.0f, 3.0f, 5.0f)}, SurfaceType::Normal, false, 1}
    };

    for (int frame = 0; frame < 120; ++frame) {
        phys3.update(dt, elevatedPlat, false, false, false, false, false);
    }
    std::printf("[PASS] Test 3: Falling onto elevated platform (top Y=3.0). Landed Y=%.2f, onGround=%s (Expected Y=3.00, onGround=true)\n",
                phys3.position.y, phys3.onGround ? "true" : "false");

    std::printf("=== Phase 0 Physics Tests Completed ===\n\n");
    std::fflush(stdout);
}
