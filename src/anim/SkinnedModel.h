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

struct SkeletonNode {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    glm::vec3 localTranslation{0.0f};
    glm::quat localRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 localScale{1.0f};
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
    void updateSkeleton(const AnimationClip* clip, float time);
    void updateSkeletonCrossfade(const AnimationClip* clipA, float timeA,
                                 const AnimationClip* clipB, float timeB,
                                 float blendFactor);

    void draw(GLuint program, const glm::mat4& modelMatrix) const;

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
