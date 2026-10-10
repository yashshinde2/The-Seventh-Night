#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "Animation.h"
#include "SkinnedModel.h"
#include "../physics/PhysicsEngine.h"

enum class AnimState {
    Idle,
    Walk,
    Run,
    JumpStart,
    Air,
    Fall,
    Land,
    Sit,
    Climb
};

class Animator {
public:
    Animator() = default;

    void init(const AnimationLibrary& animLib);
    void update(float dt, const CharacterPhysics& physics, SkinnedModel& model);

    float getFacingAngleDegrees() const { return currentFacingY_; }
    AnimState getCurrentState() const { return currentState_; }
    const char* getStateName() const;

private:
    void transitionTo(AnimState newState);

    const AnimationClip* clipIdle_ = nullptr;
    const AnimationClip* clipWalk_ = nullptr;
    const AnimationClip* clipRun_ = nullptr;
    const AnimationClip* clipJumpStart_ = nullptr;
    const AnimationClip* clipAir_ = nullptr;
    const AnimationClip* clipFall_ = nullptr;
    const AnimationClip* clipLand_ = nullptr;

    AnimState currentState_ = AnimState::Idle;
    AnimState previousState_ = AnimState::Idle;

    float currentTime_ = 0.0f;
    float prevTime_ = 0.0f;

    float blendTimer_ = 1.0f;
    float blendDuration_ = 0.2f;

    bool wasOnGround_ = true;
    bool facingRight_ = true;
    float currentFacingY_ = 90.0f; // Degrees (starts facing +X: 90 deg)
    float targetFacingY_ = 90.0f;
};
