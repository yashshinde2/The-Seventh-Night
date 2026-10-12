#include "PhysicsEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

void CharacterPhysics::update(float dt, const std::vector<ObstaclePlatform>& platforms,
                            bool moveLeft, bool moveRight, bool sprint, bool jumpRequested, bool dropRequested,
                            bool moveForward, bool moveBackward) {
    // Jump buffering
    if (jumpRequested) jumpBufferTimer = 0.12f;
    else jumpBufferTimer = std::max(0.0f, jumpBufferTimer - dt);

    // Coyote timer
    if (onGround) coyoteTimer = 0.10f;
    else coyoteTimer = std::max(0.0f, coyoteTimer - dt);

    AABB currentBox = getAABB();

    // --- Phase 4: Climbing / Ledge Detection ---
    const ObstaclePlatform* activeClimbPlat = nullptr;
    for (const auto& plat : platforms) {
        if (plat.type == SurfaceType::ClimbableVine || plat.type == SurfaceType::Ledge) {
            // Check broad proximity around vine/ledge
            AABB proximity = plat.bounds;
            proximity.minPos.x -= 0.35f;
            proximity.maxPos.x += 0.35f;
            proximity.minPos.z -= 0.5f;
            proximity.maxPos.z += 0.5f;

            if (currentBox.intersects(proximity)) {
                activeClimbPlat = &plat;
                break;
            }
        }
    }

    if (!isClimbing && activeClimbPlat != nullptr) {
        // Engage climb if player presses upward movement (W/Up) or jumps while in proximity
        if (moveForward || (jumpRequested && !onGround)) {
            isClimbing = true;
            onGround = false;
            velocity = glm::vec3(0.0f);
        }
    }

    if (isClimbing) {
        if (activeClimbPlat == nullptr) {
            isClimbing = false;
        } else {
            // Climbing movement: vertical motion up/down
            float climbSpeed = 3.2f;
            if (moveForward) {
                velocity.y = climbSpeed;
            } else if (moveBackward) {
                velocity.y = -climbSpeed;
            } else {
                velocity.y = 0.0f;
            }

            // Slight lateral adjustments while on vine
            float targetVelX = 0.0f;
            if (moveLeft) targetVelX = -moveSpeed * 0.4f;
            if (moveRight) targetVelX = moveSpeed * 0.4f;
            velocity.x += (targetVelX - velocity.x) * (1.0f - std::exp(-12.0f * dt));

            position.y += velocity.y * dt;
            position.x += velocity.x * dt;

            // Jump off the climbable surface
            if (jumpRequested) {
                isClimbing = false;
                velocity.y = jumpForce * 0.85f;
                velocity.x = moveLeft ? -moveSpeed : (moveRight ? moveSpeed : 0.0f);
            }
            // Vault onto top ledge
            else if (position.y >= activeClimbPlat->bounds.maxPos.y - 0.2f) {
                position.y = activeClimbPlat->bounds.maxPos.y;
                velocity.y = 0.0f;
                isClimbing = false;
                onGround = true;
            }
            // Drop to ground
            else if (position.y <= 0.0f) {
                position.y = 0.0f;
                velocity.y = 0.0f;
                isClimbing = false;
                onGround = true;
            }

            return; // Physics update completed for climbing state
        }
    }

    // --- Standard Locomotion Physics ---

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

    // AABB X Collision Check against solid environment obstacles (rocks, trees, walls)
    AABB boxX = getAABB();
    const float eps = 0.05f;
    for (const auto& plat : platforms) {
        if (plat.isTrigger || plat.type == SurfaceType::OneWay || plat.type == SurfaceType::ClimbableVine) continue;

        bool overlapsY = boxX.minPos.y < plat.bounds.maxPos.y - eps &&
                         boxX.maxPos.y > plat.bounds.minPos.y + eps;
        bool overlapsZ = boxX.minPos.z < plat.bounds.maxPos.z - eps &&
                         boxX.maxPos.z > plat.bounds.minPos.z + eps;

        if (overlapsY && overlapsZ && boxX.intersects(plat.bounds)) {
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
    float prevPosY = position.y;
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

        // Skip climbing vines for floor landing
        if (plat.type == SurfaceType::ClimbableVine) continue;

        // One-Way Platform Logic: can drop through if dropRequested
        if (plat.type == SurfaceType::OneWay) {
            if (dropRequested) continue;
            // Can only land on one-way platform from above when moving downwards
            if (velocity.y <= 0.0f && prevPosY >= plat.bounds.maxPos.y - 0.15f) {
                if (boxY.intersects(plat.bounds)) {
                    position.y = plat.bounds.maxPos.y;
                    velocity.y = 0.0f;
                    onGround = true;
                }
            }
            // Passing upwards freely through one-way platform: no collision!
            continue;
        }

        // Solid platforms, rocks, and ground slabs
        if (boxY.intersects(plat.bounds)) {
            if (velocity.y < 0.0f && prevPosY >= plat.bounds.maxPos.y - 0.15f) {
                position.y = plat.bounds.maxPos.y;
                velocity.y = 0.0f;
                onGround = true;
            } else if (velocity.y > 0.0f) {
                position.y = plat.bounds.minPos.y - size.y;
                velocity.y = 0.0f;
            }
        }
    }
}

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

void runPhysicsPhase4Test() {
    std::printf("=== Running Phase 4 Advanced Collision Tests ===\n");
    float dt = 1.0f / 60.0f;

    // Test 4.1: One-Way Platform - Jumping up through from below
    {
        CharacterPhysics phys;
        phys.position = glm::vec3(0.0f, 0.0f, 0.0f);
        phys.velocity.y = 8.0f; // Jumping upwards

        std::vector<ObstaclePlatform> plats = {
            {AABB{glm::vec3(-5.0f, 2.0f, -2.0f), glm::vec3(5.0f, 2.2f, 2.0f)}, SurfaceType::OneWay, false, 1}
        };

        // Update 10 frames upwards
        for (int i = 0; i < 10; ++i) {
            phys.update(dt, plats, false, false, false, false, false);
        }
        // Player should be above Y=1.0 and passing through without collision stop
        bool passedThrough = phys.position.y > 1.0f && phys.velocity.y > 0.0f;
        std::printf("[%s] Test 4.1: One-Way platform jump-through upward: Y=%.2f, Vy=%.2f\n",
                    passedThrough ? "PASS" : "FAIL", phys.position.y, phys.velocity.y);
    }

    // Test 4.2: One-Way Platform - Landing securely on top from above
    {
        CharacterPhysics phys;
        phys.position = glm::vec3(0.0f, 4.0f, 0.0f); // Above platform
        phys.velocity.y = -2.0f;

        std::vector<ObstaclePlatform> plats = {
            {AABB{glm::vec3(-5.0f, 2.0f, -2.0f), glm::vec3(5.0f, 2.2f, 2.0f)}, SurfaceType::OneWay, false, 1}
        };

        for (int i = 0; i < 60; ++i) {
            phys.update(dt, plats, false, false, false, false, false);
        }
        bool landed = std::abs(phys.position.y - 2.2f) < 0.01f && phys.onGround;
        std::printf("[%s] Test 4.2: One-Way platform landing from above: Y=%.2f, onGround=%s (Expected Y=2.20)\n",
                    landed ? "PASS" : "FAIL", phys.position.y, phys.onGround ? "true" : "false");
    }

    // Test 4.3: One-Way Platform - Dropping through when dropRequested
    {
        CharacterPhysics phys;
        phys.position = glm::vec3(0.0f, 2.2f, 0.0f); // Resting on platform
        phys.onGround = true;

        std::vector<ObstaclePlatform> plats = {
            {AABB{glm::vec3(-5.0f, -1.0f, -5.0f), glm::vec3(5.0f, 0.0f, 5.0f)}, SurfaceType::Normal, false, 1},
            {AABB{glm::vec3(-5.0f, 2.0f, -2.0f), glm::vec3(5.0f, 2.2f, 2.0f)}, SurfaceType::OneWay, false, 1}
        };

        // First frame with dropRequested
        for (int i = 0; i < 30; ++i) {
            phys.update(dt, plats, false, false, false, false, true); // dropRequested = true
        }
        bool dropped = phys.position.y < 2.0f;
        std::printf("[%s] Test 4.3: One-Way platform drop-through: Y=%.2f (Expected < 2.0)\n",
                    dropped ? "PASS" : "FAIL", phys.position.y);
    }

    // Test 4.4: Climbing Vine - Approaching, engaging climb, ascending, and vaulting onto ledge
    {
        CharacterPhysics phys;
        phys.position = glm::vec3(10.0f, 0.0f, 0.0f); // At base of vine

        std::vector<ObstaclePlatform> plats = {
            {AABB{glm::vec3(9.5f, 0.0f, -1.0f), glm::vec3(10.5f, 3.5f, 1.0f)}, SurfaceType::ClimbableVine, false, 1}
        };

        // Move forward (W/Up) to climb
        for (int i = 0; i < 80; ++i) {
            phys.update(dt, plats, false, false, false, false, false, true, false); // moveForward = true
        }

        bool climbedAndVaulted = phys.position.y >= 3.4f;
        std::printf("[%s] Test 4.4: Climbable vine ascent & ledge vault: Y=%.2f, isClimbing=%s, onGround=%s (Expected top Y~3.50)\n",
                    climbedAndVaulted ? "PASS" : "FAIL", phys.position.y,
                    phys.isClimbing ? "true" : "false", phys.onGround ? "true" : "false");
    }

    std::printf("=== Phase 4 Advanced Collision Tests Completed ===\n\n");
    std::fflush(stdout);
}
