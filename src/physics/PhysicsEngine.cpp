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
    for (const auto& plat : platforms) {
        if (plat.isTrigger) continue;
        if (boxX.intersects(plat.bounds)) {
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
