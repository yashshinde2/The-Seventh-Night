#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>

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
    std::vector<KeyframeVec3> translationKeys;
    std::vector<KeyframeQuat> rotationKeys;
    std::vector<KeyframeVec3> scaleKeys;

    glm::vec3 sampleTranslation(float time) const;
    glm::quat sampleRotation(float time) const;
    glm::vec3 sampleScale(float time) const;
};

class AnimationClip {
public:
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
    std::unordered_map<std::string, size_t> nodeToChannel;

    void buildChannelMap();
    const AnimationChannel* getChannel(const std::string& nodeName) const;
};

class AnimationLibrary {
public:
    std::vector<AnimationClip> clips;
    std::unordered_map<std::string, size_t> clipMap;

    bool loadGLBAnimations(const std::string& glbPath);
    const AnimationClip* findClip(const std::string& name) const;
    void printClipNames() const;
};
