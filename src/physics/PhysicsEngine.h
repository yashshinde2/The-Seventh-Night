#pragma once

#include <glm/glm.hpp>
#include <vector>

enum class SurfaceType {
    Normal,
    Slippery,
    OneWay,
    Ledge,
    ClimbableVine
};

struct AABB {
    glm::vec3 minPos{0.0f};
    glm::vec3 maxPos{0.0f};

    bool intersects(const AABB& other) const {
        return (minPos.x <= other.maxPos.x && maxPos.x >= other.minPos.x) &&
               (minPos.y <= other.maxPos.y && maxPos.y >= other.minPos.y) &&
               (minPos.z <= other.maxPos.z && maxPos.z >= other.minPos.z);
    }
};

struct ObstaclePlatform {
    AABB bounds;
    SurfaceType type = SurfaceType::Normal;
    bool isTrigger = false;
    int lane = 1; // 0 = FG, 1 = MID, 2 = BG
};

class CharacterPhysics {
public:
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 size{0.6f, 1.7f, 0.4f}; // Aarav bounding box size

    int targetLane = 1;
    float currentLaneZ = 0.0f;
    const float laneZValues[3] = {2.0f, 0.0f, -2.0f};

    bool onGround = false;
    bool isClimbing = false;
    bool isSliding = false;
    bool isGroundedOnOneWay = false;

    float moveSpeed = 4.5f;
    float sprintMultiplier = 1.6f;
    float jumpForce = 7.0f;
    float gravity = 18.0f;

    AABB getAABB() const {
        glm::vec3 half = size * 0.5f;
        return AABB{
            glm::vec3(position.x - half.x, position.y, currentLaneZ - half.z),
            glm::vec3(position.x + half.x, position.y + size.y, currentLaneZ + half.z)
        };
    }

    void update(float dt, const std::vector<ObstaclePlatform>& platforms, bool moveLeft, bool moveRight, bool sprint, bool jumpRequested, bool dropRequested);
};
