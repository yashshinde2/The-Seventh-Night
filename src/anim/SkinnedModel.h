#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>

#include "Animation.h"

struct SkinnedMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    GLuint texture = 0;
    glm::vec4 color{1.0f};
};

struct NodeTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Pose {
    std::vector<NodeTransform> nodeTransforms;
};

struct SkeletonNode {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    NodeTransform bindPose;
    NodeTransform currentTransform;
    glm::mat4 globalTransform{1.0f};
};

struct Joint {
    int nodeIndex = -1;
    std::string name;
    glm::mat4 inverseBindMatrix{1.0f};
};

class SkinnedModel {
public:
    SkinnedModel() = default;
    ~SkinnedModel();

    SkinnedModel(const SkinnedModel&) = delete;
    SkinnedModel& operator=(const SkinnedModel&) = delete;
    SkinnedModel(SkinnedModel&& other) noexcept;
    SkinnedModel& operator=(SkinnedModel&& other) noexcept;

    bool loadGLB(const std::string& path);

    // Phase 3: Bind pose, Pose sampling, blending, and application
    Pose getBindPose() const;
    Pose samplePose(const AnimationClip* clip, float time, bool loop = true) const;
    static Pose blendPoses(const Pose& a, const Pose& b, float t);
    void applyPose(const Pose& pose);

    // Legacy update fallback
    void updateSkeleton(const AnimationClip* clip, float time, bool loop = true);

    void draw(GLuint program, const glm::mat4& modelMatrix) const;

    glm::mat4 getBoneWorldTransform(const std::string& name, const glm::mat4& modelMatrix) const;
    std::vector<std::string> getBoneNames() const;
    const std::vector<Joint>& getJoints() const { return joints_; }

private:
    void release();
    void computeGlobalTransforms(int nodeIdx, const glm::mat4& parentGlobal);

    std::vector<SkinnedMesh> meshes_;
    std::vector<SkeletonNode> nodes_;
    std::unordered_map<std::string, int> nameToNode_;
    std::vector<Joint> joints_;
    std::vector<glm::mat4> jointMatrices_;
};
