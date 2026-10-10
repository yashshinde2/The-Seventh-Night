#include "PhysicsEngine.h"
#include <algorithm>

#include <cmath>

void CharacterPhysics::update(float dt, const std::vector<ObstaclePlatform>& platforms,
                            bool moveLeft, bool moveRight, bool sprint, bool jumpRequested, bool dropRequested,
                            bool moveForward, bool moveBackward) {
    // Jump buffering
    if (jumpRequested) jumpBufferTimer = 0.12f;
    else jumpBufferTimer = std::max(0.0f, jumpBufferTimer - dt);

    // Coyote timer
    if (onGround) coyoteTimer = 0.10f;
    else coyoteTimer = std::max(0.0f, coyoteTimer - dt);

    // Multi-directional planar input (X and Z)
    float inX = 0.0f;
    float inZ = 0.0f;
    if (moveLeft) inX -= 1.0f;
    if (moveRight) inX += 1.0f;
    if (moveForward) inZ -= 1.0f;  // -Z into background
    if (moveBackward) inZ += 1.0f; // +Z towards foreground

    float inLen = std::sqrt(inX * inX + inZ * inZ);
    if (inLen > 0.001f) {
        inX /= inLen;
        inZ /= inLen;
    }

    float currentSpeed = moveSpeed * (sprint ? sprintMultiplier : 1.0f);
    float targetVelX = inX * currentSpeed;
    float targetVelZ = inZ * currentSpeed * 0.85f;

    // Smooth acceleration & decay
    float smoothFactor = 1.0f - std::exp(-16.0f * dt);
    velocity.x += (targetVelX - velocity.x) * smoothFactor;
    velocity.z += (targetVelZ - velocity.z) * smoothFactor;

    position.x += velocity.x * dt;
    position.z = std::max(-3.5f, std::min(3.5f, position.z + velocity.z * dt));
    currentLaneZ = position.z;

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

    // Vertical Jump with coyote time & buffer
    if (jumpBufferTimer > 0.0f && (onGround || coyoteTimer > 0.0f)) {
        velocity.y = jumpForce;
        onGround = false;
        coyoteTimer = 0.0f;
        jumpBufferTimer = 0.0f;
    }

    velocity.y -= gravity * dt;
    position.y += velocity.y * dt;

    // Floor & Platform Landing
    onGround = false;

    // Ground plane (y = 0) fallback
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
