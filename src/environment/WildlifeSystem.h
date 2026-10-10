#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

enum class BirdState {
    Perched,
    TakeOff,
    Flying,
    Circling
};

struct Bird {
    glm::vec3 pos;
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float flapTimer = 0.0f;
    BirdState state = BirdState::Perched;
    glm::vec3 perchPos;
    float flightTime = 0.0f;
    float circleCenter = 0.0f;
    float circleRadius = 8.0f;
    float circleSpeed = 1.2f;
};

enum class CreatureState {
    Grazing,
    Alert,
    Fleeing
};

struct ForestCreature {
    glm::vec3 pos;
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    CreatureState state = CreatureState::Grazing;
    float headBob = 0.0f;
    float stateTimer = 0.0f;
    float scale = 1.0f;
};

struct Firefly {
    glm::vec3 basePos;
    glm::vec3 pos;
    float phase;
    float speed;
    float radius;
};

class WildlifeSystem {
public:
    WildlifeSystem() = default;
    ~WildlifeSystem();

    void init();
    void update(float dt, const glm::vec3& playerPos);
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos);

private:
    void initBirdMesh();
    void initCreatureMesh();
    void initFireflyMesh();
    void spawnWildlife();

    GLuint wildlifeShader_ = 0;
    GLuint fireflyShader_ = 0;

    // Bird Geometry (Body, Left Wing, Right Wing)
    GLuint birdBodyVao_ = 0, birdBodyVbo_ = 0;
    GLsizei birdBodyVerts_ = 0;
    GLuint birdWingVao_ = 0, birdWingVbo_ = 0;
    GLsizei birdWingVerts_ = 0;

    // Forest Creature Geometry (Low-Poly Stag with Antlers)
    GLuint creatureVao_ = 0, creatureVbo_ = 0;
    GLsizei creatureVerts_ = 0;

    // Fireflies / Golden Embers
    GLuint fireflyVao_ = 0, fireflyVbo_ = 0;
    GLsizei fireflyVerts_ = 0;

    std::vector<Bird> birds_;
    std::vector<ForestCreature> creatures_;
    std::vector<Firefly> fireflies_;

    float time_ = 0.0f;
};
