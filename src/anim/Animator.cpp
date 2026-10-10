#include "Animator.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

const char* Animator::getStateName() const {
    switch (currentState_) {
        case AnimState::Idle: return "Idle";
        case AnimState::Walk: return "Walk";
        case AnimState::Run: return "Run";
        case AnimState::JumpStart: return "JumpStart";
        case AnimState::Air: return "Air";
        case AnimState::Fall: return "Fall";
        case AnimState::Land: return "Land";
        case AnimState::Sit: return "Sit";
        case AnimState::Climb: return "Climb";
    }
    return "Unknown";
}

void Animator::init(const AnimationLibrary& animLib) {
    clipIdle_ = animLib.findClipAny({"Idle_Loop", "Idle_Lantern_Loop", "Idle"}, "Idle");
    clipWalk_ = animLib.findClipAny({"Walk_Loop", "Walk"}, "Walk");
    clipRun_  = animLib.findClipAny({"Jog_Fwd_Loop", "Sprint_Loop", "Jog", "Sprint"}, "Run");
    clipJumpStart_ = animLib.findClipAny({"Jump_Start", "NinjaJump_Start"}, "JumpStart");
    clipAir_ = animLib.findClipAny({"Jump_Loop", "NinjaJump_Idle_Loop"}, "Air");
    clipFall_ = animLib.findClipAny({"Jump_Loop", "NinjaJump_Idle_Loop"}, "Fall");
    clipLand_ = animLib.findClipAny({"Jump_Land", "NinjaJump_Land"}, "Land");

    // Strip root motion on locomotion clips so physics moves the character
    if (clipWalk_) const_cast<AnimationClip*>(clipWalk_)->stripRootMotion();
    if (clipRun_)  const_cast<AnimationClip*>(clipRun_)->stripRootMotion();

    currentState_ = AnimState::Idle;
    previousState_ = AnimState::Idle;
    currentTime_ = 0.0f;
    prevTime_ = 0.0f;
    blendTimer_ = blendDuration_; // No blend initially
}

void Animator::transitionTo(AnimState newState) {
    if (currentState_ == newState) return;

    previousState_ = currentState_;
    currentState_ = newState;

    prevTime_ = currentTime_;
    currentTime_ = 0.0f;
    blendTimer_ = 0.0f; // Start crossfade
}

static const AnimationClip* getClipForState(AnimState state,
                                            const AnimationClip* idle,
                                            const AnimationClip* walk,
                                            const AnimationClip* run,
                                            const AnimationClip* jStart,
                                            const AnimationClip* air,
                                            const AnimationClip* fall,
                                            const AnimationClip* land) {
    switch (state) {
        case AnimState::Idle: return idle;
        case AnimState::Walk: return walk ? walk : idle;
        case AnimState::Run: return run ? run : (walk ? walk : idle);
        case AnimState::JumpStart: return jStart ? jStart : air;
        case AnimState::Air: return air ? air : idle;
        case AnimState::Fall: return fall ? fall : air;
        case AnimState::Land: return land ? land : idle;
        default: return idle;
    }
}

static bool isLoopingState(AnimState state) {
    return (state == AnimState::Idle || state == AnimState::Walk || state == AnimState::Run || state == AnimState::Air || state == AnimState::Fall);
}

void Animator::update(float dt, const CharacterPhysics& physics, SkinnedModel& model) {
    float horizSpeed = std::sqrt(physics.velocity.x * physics.velocity.x + physics.velocity.z * physics.velocity.z);

    // Update facing direction based on 2D planar velocity (+Z camera, -Z depth, +X right, -X left)
    if (horizSpeed > 0.15f) {
        float angleRad = std::atan2(physics.velocity.x, physics.velocity.z);
        targetFacingY_ = glm::degrees(angleRad);
    }

    // Smooth shortest-arc facing turn
    float diff = targetFacingY_ - currentFacingY_;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    currentFacingY_ += diff * (1.0f - std::exp(-18.0f * dt));

    // Determine state from physics
    if (!physics.onGround) {
        if (physics.velocity.y > 1.5f) {
            transitionTo(AnimState::Air);
        } else {
            transitionTo(AnimState::Fall);
        }
    } else {
        // Just landed event
        if (!wasOnGround_) {
            transitionTo(AnimState::Land);
        } else if (currentState_ == AnimState::Land) {
            // Stay in land pose for short duration before returning to locomotion
            if (currentTime_ > 0.22f) {
                transitionTo(horizSpeed > 4.8f ? AnimState::Run : (horizSpeed > 0.2f ? AnimState::Walk : AnimState::Idle));
            }
        } else {
            if (horizSpeed > 4.8f) {
                transitionTo(AnimState::Run);
            } else if (horizSpeed > 0.2f) {
                transitionTo(AnimState::Walk);
            } else {
                transitionTo(AnimState::Idle);
            }
        }
    }
    wasOnGround_ = physics.onGround;

    // Advance playback timers with locomotion speed scaling
    float playRate = 1.0f;
    if (currentState_ == AnimState::Walk) playRate = std::max(0.45f, horizSpeed / 3.8f);
    else if (currentState_ == AnimState::Run) playRate = std::max(0.6f, horizSpeed / 6.2f);

    currentTime_ += dt * playRate;
    prevTime_ += dt;

    // Sample current pose
    const AnimationClip* currClip = getClipForState(currentState_, clipIdle_, clipWalk_, clipRun_, clipJumpStart_, clipAir_, clipFall_, clipLand_);
    bool currLoop = isLoopingState(currentState_);
    Pose currPose = model.samplePose(currClip, currentTime_, currLoop);

    // Apply pose (with crossfade blending if active)
    if (blendTimer_ < blendDuration_) {
        blendTimer_ += dt;
        float factor = glm::clamp(blendTimer_ / blendDuration_, 0.0f, 1.0f);

        const AnimationClip* prevClip = getClipForState(previousState_, clipIdle_, clipWalk_, clipRun_, clipJumpStart_, clipAir_, clipFall_, clipLand_);
        bool prevLoop = isLoopingState(previousState_);
        Pose prevPose = model.samplePose(prevClip, prevTime_, prevLoop);

        Pose finalPose = SkinnedModel::blendPoses(prevPose, currPose, factor);
        model.applyPose(finalPose);
    } else {
        model.applyPose(currPose);
    }
}
