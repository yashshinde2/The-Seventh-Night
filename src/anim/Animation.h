#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>

enum class AnimInterpolation {
    Linear,
    Step,
    CubicSpline
};

struct KeyframeVec3 {
    float time = 0.0f;
    glm::vec3 value{0.0f};
};

struct KeyframeQuat {
    float time = 0.0f;
    glm::quat value{1.0f, 0.0f, 0.0f, 0.0f};
};

struct AnimationChannel {
    std::string targetNodeName;
    AnimInterpolation interp = AnimInterpolation::Linear;
    std::vector<KeyframeVec3> translationKeys;
    std::vector<KeyframeQuat> rotationKeys;
    std::vector<KeyframeVec3> scaleKeys;

    bool hasTranslation() const { return !translationKeys.empty(); }
    bool hasRotation() const { return !rotationKeys.empty(); }
    bool hasScale() const { return !scaleKeys.empty(); }

    glm::vec3 sampleTranslation(float time, const glm::vec3& fallback) const;
    glm::quat sampleRotation(float time, const glm::quat& fallback) const;
    glm::vec3 sampleScale(float time, const glm::vec3& fallback) const;
};

class AnimationClip {
public:
    std::string name;
    float duration = 0.0f;
    bool loop = true;
    std::vector<AnimationChannel> channels;
    std::unordered_map<std::string, size_t> nodeToChannel;

    void buildChannelMap();
    const AnimationChannel* getChannel(const std::string& nodeName) const;
    void stripRootMotion(const std::string& rootBoneName = "Root");
};

class AnimationLibrary {
public:
    std::deque<AnimationClip> clips; // Stable storage: references and pointers never invalidate
    std::unordered_map<std::string, size_t> clipMap;

    bool loadGLBAnimations(const std::string& glbPath);
    const AnimationClip* findClip(const std::string& name) const;
    const AnimationClip* findClipAny(const std::vector<std::string>& candidateNames, const std::string& roleDesc = "") const;
    void printClipNames() const;

    float checkRetarget(const AnimationClip& clip, const std::vector<std::string>& skeletonBoneNames) const;
};
