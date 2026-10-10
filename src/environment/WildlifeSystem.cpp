#include "WildlifeSystem.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Wildlife shader error: %s\n", log);
    }
    return s;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static const char* kWildlifeVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vPos;
out vec3 vNrm;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vPos = wp.xyz;
    vNrm = mat3(transpose(inverse(uModel))) * aNrm;
    gl_Position = uProj * uView * wp;
}
)";

static const char* kWildlifeFS = R"(
#version 330 core
in vec3 vPos;
in vec3 vNrm;

uniform vec3 uColor;
uniform vec3 uCamPos;

out vec4 frag;

void main() {
    vec3 n = normalize(vNrm);
    if (!gl_FrontFacing) n = -n;
    
    // Cool directional moonlight
    vec3 moonDir = normalize(vec3(-0.4, 1.0, 0.6));
    float diff = max(dot(n, moonDir), 0.0);
    vec3 ambient = vec3(0.24, 0.28, 0.36);
    vec3 moon = vec3(0.38, 0.48, 0.68) * diff;
    
    vec3 col = uColor * (ambient + moon);
    
    // Distance fog blend
    float fd = length(uCamPos - vPos);
    float fog = 1.0 - exp(-pow(fd * 0.020, 1.3));
    vec3 fogCol = vec3(0.08, 0.12, 0.18);
    col = mix(col, fogCol, fog);
    
    frag = vec4(col, 1.0);
}
)";

// Firefly Billboard Shader
static const char* kFireflyVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vUV;
out vec3 vPos;

void main() {
    vUV = aUV;
    vec4 wp = uModel * vec4(aPos, 1.0);
    vPos = wp.xyz;
    gl_Position = uProj * uView * wp;
}
)";

static const char* kFireflyFS = R"(
#version 330 core
in vec2 vUV;
in vec3 vPos;

uniform vec3 uCamPos;
uniform float uPulse;

out vec4 frag;

void main() {
    float d = length(vUV * 2.0 - 1.0);
    float core = smoothstep(0.45, 0.0, d);
    float glow = exp(-d * 3.2) * 0.85;
    
    vec3 amberGold = vec3(1.0, 0.82, 0.35) * uPulse * 2.2;
    float alpha = clamp(core + glow * uPulse, 0.0, 1.0);
    
    if (alpha < 0.02) discard;
    frag = vec4(amberGold, alpha);
}
)";

} // namespace

WildlifeSystem::~WildlifeSystem() {
    if (wildlifeShader_) glDeleteProgram(wildlifeShader_);
    if (fireflyShader_) glDeleteProgram(fireflyShader_);
    if (birdBodyVao_) glDeleteVertexArrays(1, &birdBodyVao_);
    if (birdBodyVbo_) glDeleteBuffers(1, &birdBodyVbo_);
    if (birdWingVao_) glDeleteVertexArrays(1, &birdWingVao_);
    if (birdWingVbo_) glDeleteBuffers(1, &birdWingVbo_);
    if (creatureVao_) glDeleteVertexArrays(1, &creatureVao_);
    if (creatureVbo_) glDeleteBuffers(1, &creatureVbo_);
    if (fireflyVao_) glDeleteVertexArrays(1, &fireflyVao_);
    if (fireflyVbo_) glDeleteBuffers(1, &fireflyVbo_);
}

void WildlifeSystem::init() {
    wildlifeShader_ = linkProgram(kWildlifeVS, kWildlifeFS);
    fireflyShader_ = linkProgram(kFireflyVS, kFireflyFS);

    initBirdMesh();
    initCreatureMesh();
    initFireflyMesh();
    spawnWildlife();
}

void WildlifeSystem::initBirdMesh() {
    // 1. Sleek 3D Crow/Raven Body
    std::vector<float> bodyVerts = {
        // Head / Beak (facing +Z)
         0.0f,  0.08f,  0.35f,   0.0f,  0.7f,  0.7f,
        -0.1f, -0.05f,  0.10f,  -0.7f, -0.4f,  0.5f,
         0.1f, -0.05f,  0.10f,   0.7f, -0.4f,  0.5f,

        // Torso Top
         0.0f,  0.12f,  0.0f,    0.0f,  1.0f,  0.0f,
        -0.12f, 0.0f,  -0.15f,  -0.8f,  0.5f, -0.3f,
         0.12f, 0.0f,  -0.15f,   0.8f,  0.5f, -0.3f,

        // Torso Bottom
         0.0f, -0.10f,  0.0f,    0.0f, -1.0f,  0.0f,
         0.12f, 0.0f,  -0.15f,   0.8f, -0.5f, -0.3f,
        -0.12f, 0.0f,  -0.15f,  -0.8f, -0.5f, -0.3f,

        // Tail (extending -Z)
         0.0f,  0.04f, -0.15f,   0.0f,  0.6f, -0.8f,
        -0.08f, 0.0f,  -0.45f,  -0.5f,  0.2f, -0.8f,
         0.08f, 0.0f,  -0.45f,   0.5f,  0.2f, -0.8f
    };
    birdBodyVerts_ = (GLsizei)(bodyVerts.size() / 6);

    glGenVertexArrays(1, &birdBodyVao_);
    glGenBuffers(1, &birdBodyVbo_);
    glBindVertexArray(birdBodyVao_);
    glBindBuffer(GL_ARRAY_BUFFER, birdBodyVbo_);
    glBufferData(GL_ARRAY_BUFFER, bodyVerts.size() * sizeof(float), bodyVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    // 2. Articulated Bird Wing (attached at root (0,0,0) extending along +X)
    std::vector<float> wingVerts = {
        // Base triangle
        0.0f,  0.02f,  0.10f,   0.0f, 1.0f, 0.0f,
        0.45f, 0.05f, -0.05f,   0.0f, 1.0f, 0.0f,
        0.0f,  0.00f, -0.15f,   0.0f, 1.0f, 0.0f,

        // Wingtip triangle
        0.45f, 0.05f, -0.05f,   0.0f, 1.0f, 0.0f,
        0.85f, 0.02f, -0.18f,   0.0f, 1.0f, 0.0f,
        0.30f, 0.00f, -0.22f,   0.0f, 1.0f, 0.0f
    };
    birdWingVerts_ = (GLsizei)(wingVerts.size() / 6);

    glGenVertexArrays(1, &birdWingVao_);
    glGenBuffers(1, &birdWingVbo_);
    glBindVertexArray(birdWingVao_);
    glBindBuffer(GL_ARRAY_BUFFER, birdWingVbo_);
    glBufferData(GL_ARRAY_BUFFER, wingVerts.size() * sizeof(float), wingVerts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void WildlifeSystem::initCreatureMesh() {
    // Low-poly Stylized Sahyadri Stag / Forest Deer with Antlers
    std::vector<float> verts;

    auto addBox = [&](glm::vec3 c, glm::vec3 s) {
        glm::vec3 h = s * 0.5f;
        // 6 faces
        struct Face { glm::vec3 n, u, v; };
        Face faces[6] = {
            {{ 1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
            {{-1, 0, 0}, {0, 1, 0}, {0, 0,-1}},
            {{ 0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
            {{ 0,-1, 0}, {1, 0, 0}, {0, 0,-1}},
            {{ 0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
            {{ 0, 0,-1}, {-1, 0, 0}, {0, 1, 0}}
        };
        for (const auto& f : faces) {
            glm::vec3 p0 = c + f.n * h + (-f.u - f.v) * h;
            glm::vec3 p1 = c + f.n * h + ( f.u - f.v) * h;
            glm::vec3 p2 = c + f.n * h + ( f.u + f.v) * h;
            glm::vec3 p3 = c + f.n * h + (-f.u + f.v) * h;
            // Tri 1
            verts.insert(verts.end(), {p0.x, p0.y, p0.z, f.n.x, f.n.y, f.n.z});
            verts.insert(verts.end(), {p1.x, p1.y, p1.z, f.n.x, f.n.y, f.n.z});
            verts.insert(verts.end(), {p2.x, p2.y, p2.z, f.n.x, f.n.y, f.n.z});
            // Tri 2
            verts.insert(verts.end(), {p0.x, p0.y, p0.z, f.n.x, f.n.y, f.n.z});
            verts.insert(verts.end(), {p2.x, p2.y, p2.z, f.n.x, f.n.y, f.n.z});
            verts.insert(verts.end(), {p3.x, p3.y, p3.z, f.n.x, f.n.y, f.n.z});
        }
    };

    // Body Torso
    addBox(glm::vec3(0.0f, 1.15f, 0.0f), glm::vec3(0.55f, 0.50f, 1.25f));
    // 4 Slender Legs
    addBox(glm::vec3(-0.20f, 0.45f,  0.42f), glm::vec3(0.10f, 0.90f, 0.10f));
    addBox(glm::vec3( 0.20f, 0.45f,  0.42f), glm::vec3(0.10f, 0.90f, 0.10f));
    addBox(glm::vec3(-0.20f, 0.45f, -0.42f), glm::vec3(0.10f, 0.90f, 0.10f));
    addBox(glm::vec3( 0.20f, 0.45f, -0.42f), glm::vec3(0.10f, 0.90f, 0.10f));
    // Angled Neck
    addBox(glm::vec3(0.0f, 1.55f, 0.55f), glm::vec3(0.22f, 0.55f, 0.28f));
    // Head & Muzzle
    addBox(glm::vec3(0.0f, 1.80f, 0.72f), glm::vec3(0.24f, 0.26f, 0.40f));
    // Antlers (Left & Right Branching Spikes)
    addBox(glm::vec3(-0.20f, 2.15f, 0.65f), glm::vec3(0.05f, 0.50f, 0.06f));
    addBox(glm::vec3( 0.20f, 2.15f, 0.65f), glm::vec3(0.05f, 0.50f, 0.06f));
    addBox(glm::vec3(-0.35f, 2.30f, 0.72f), glm::vec3(0.28f, 0.05f, 0.05f));
    addBox(glm::vec3( 0.35f, 2.30f, 0.72f), glm::vec3(0.28f, 0.05f, 0.05f));

    creatureVerts_ = (GLsizei)(verts.size() / 6);

    glGenVertexArrays(1, &creatureVao_);
    glGenBuffers(1, &creatureVbo_);
    glBindVertexArray(creatureVao_);
    glBindBuffer(GL_ARRAY_BUFFER, creatureVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void WildlifeSystem::initFireflyMesh() {
    float s = 0.22f;
    std::vector<float> verts = {
        -s, -s, 0.0f,  0.0f, 0.0f,
         s, -s, 0.0f,  1.0f, 0.0f,
         s,  s, 0.0f,  1.0f, 1.0f,
        -s, -s, 0.0f,  0.0f, 0.0f,
         s,  s, 0.0f,  1.0f, 1.0f,
        -s,  s, 0.0f,  0.0f, 1.0f
    };
    fireflyVerts_ = 6;

    glGenVertexArrays(1, &fireflyVao_);
    glGenBuffers(1, &fireflyVbo_);
    glBindVertexArray(fireflyVao_);
    glBindBuffer(GL_ARRAY_BUFFER, fireflyVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void WildlifeSystem::spawnWildlife() {
    // 1. Perched Crows / Ravens (On roofs, chimneys, wagon tops, fence posts)
    std::vector<glm::vec3> perchPositions = {
        {-20.0f, 4.8f, -3.2f}, // Outpost roof apex
        {-18.5f, 4.3f, -3.2f}, // Outpost chimney rim
        {-16.2f, 1.3f, -1.2f}, // Village wagon seat
        {-5.0f,  4.9f, -3.4f}, // Elder cottage roof apex
        {-8.5f,  1.2f, -1.8f}, // Wooden fence post
        { 4.5f,  2.2f, -2.6f}, // Sacred shrine brick pillar
        { 19.0f, 4.8f, -3.2f}, // Gatehouse roof
        { 26.0f, 1.2f,  2.8f}, // Roadside fence post
        { 42.0f, 5.5f, -3.5f}, // Ancient dead tree branch
        { 68.0f, 6.2f, -4.2f}, // Deep forest twisted tree limb
        {-52.0f, 7.5f, -4.5f}  // City brick cornice
    };

    for (size_t i = 0; i < perchPositions.size(); ++i) {
        Bird b;
        b.pos = perchPositions[i];
        b.perchPos = perchPositions[i];
        b.yaw = (float)((i * 57) % 360);
        b.state = BirdState::Perched;
        birds_.push_back(b);
    }

    // High circling flock in the night sky
    for (int i = 0; i < 4; ++i) {
        Bird b;
        b.pos = glm::vec3(15.0f + i * 2.0f, 14.0f + i * 1.2f, -10.0f);
        b.state = BirdState::Circling;
        b.circleCenter = 15.0f;
        b.circleRadius = 12.0f + i * 2.0f;
        b.circleSpeed = 0.9f + i * 0.15f;
        birds_.push_back(b);
    }

    // 2. Forest Deer / Stags in the mist (Grazing in the background grove)
    std::vector<glm::vec3> creaturePositions = {
        {-32.0f, 0.0f, -5.5f}, // Transition grove
        { 12.0f, 0.0f, -4.8f}, // Meadow clearing behind shrine
        { 48.0f, 0.0f, -5.2f}, // Kaalvan ancient woods
        { 82.0f, 0.0f, -5.8f}  // Deep enchanted forest
    };

    for (size_t i = 0; i < creaturePositions.size(); ++i) {
        ForestCreature c;
        c.pos = creaturePositions[i];
        c.yaw = (i % 2 == 0) ? 90.0f : -90.0f;
        c.state = CreatureState::Grazing;
        c.scale = 0.85f + 0.15f * (i % 3);
        creatures_.push_back(c);
    }

    // 3. Swarm of Bioluminescent Fireflies / Golden Embers
    std::vector<glm::vec3> clusters = {
        {  6.0f, 1.2f, -1.8f }, // Village Sacred Shrine
        { -4.0f, 1.5f, -2.5f }, // Elder Cottage garden
        { 38.0f, 1.6f, -2.0f }, // Ancient mossy rocks
        { 65.0f, 1.8f, -2.2f }  // Deep Kaalvan clearing
    };

    for (const auto& cl : clusters) {
        for (int i = 0; i < 9; ++i) {
            Firefly f;
            f.basePos = cl + glm::vec3((float)(std::rand()%100 - 50)/25.0f,
                                       (float)(std::rand()%100)/50.0f,
                                       (float)(std::rand()%100 - 50)/25.0f);
            f.pos = f.basePos;
            f.phase = (float)std::rand() / RAND_MAX * 6.28f;
            f.speed = 0.8f + ((float)std::rand() / RAND_MAX) * 0.8f;
            f.radius = 0.6f + ((float)std::rand() / RAND_MAX) * 0.9f;
            fireflies_.push_back(f);
        }
    }
}

void WildlifeSystem::update(float dt, const glm::vec3& playerPos) {
    time_ += dt;

    // 1. Update Birds
    for (auto& b : birds_) {
        b.flapTimer += dt;

        if (b.state == BirdState::Perched) {
            float dist = glm::length(glm::vec2(playerPos.x - b.pos.x, playerPos.z - b.pos.z));
            // Trigger take-off if Aarav approaches within 5.0 units!
            if (dist < 5.0f) {
                b.state = BirdState::TakeOff;
                b.flightTime = 0.0f;
                // Launch away into the sky
                float fleeDir = (b.pos.x > playerPos.x) ? 1.0f : -1.0f;
                b.velocity = glm::vec3(fleeDir * 4.5f, 5.0f, -1.5f);
                b.yaw = (fleeDir > 0.0f) ? 90.0f : -90.0f;
            }
        } else if (b.state == BirdState::TakeOff) {
            b.flightTime += dt;
            b.pos += b.velocity * dt;
            b.velocity.y *= (1.0f - 0.5f * dt); // level off
            if (b.flightTime > 2.2f) {
                b.state = BirdState::Flying;
            }
        } else if (b.state == BirdState::Flying) {
            b.pos += b.velocity * dt;
            // Keep soaring smoothly across the sky
            b.velocity.y = std::sin(time_ * 2.0f) * 0.6f;
            // If flew too far, reset back to perch after time
            if (std::abs(b.pos.x - b.perchPos.x) > 60.0f) {
                b.pos = b.perchPos;
                b.state = BirdState::Perched;
            }
        } else if (b.state == BirdState::Circling) {
            float angle = time_ * b.circleSpeed;
            b.pos.x = b.circleCenter + std::cos(angle) * b.circleRadius;
            b.pos.z = -12.0f + std::sin(angle) * (b.circleRadius * 0.45f);
            b.pos.y = 15.0f + std::sin(angle * 2.0f) * 1.2f;
            b.yaw = glm::degrees(-angle) + 90.0f;
        }
    }

    // 2. Update Forest Creatures (Stags in the mist)
    for (auto& c : creatures_) {
        c.stateTimer += dt;
        float distToPlayer = glm::length(playerPos - c.pos);

        if (c.state == CreatureState::Grazing) {
            c.headBob = std::sin(time_ * 1.5f) * 0.12f;
            if (distToPlayer < 9.0f) {
                c.state = CreatureState::Alert;
                c.stateTimer = 0.0f;
                // Turn head toward Aarav
                c.yaw = (playerPos.x > c.pos.x) ? 90.0f : -90.0f;
            }
        } else if (c.state == CreatureState::Alert) {
            // If player comes closer than 5.5 units, flee into the Kaalvan shadows!
            if (distToPlayer < 5.5f) {
                c.state = CreatureState::Fleeing;
                float fleeDirX = (c.pos.x > playerPos.x) ? 1.0f : -1.0f;
                c.velocity = glm::vec3(fleeDirX * 5.2f, 0.0f, -2.5f);
                c.yaw = (fleeDirX > 0.0f) ? 110.0f : -110.0f;
            } else if (distToPlayer > 12.0f && c.stateTimer > 4.0f) {
                c.state = CreatureState::Grazing;
            }
        } else if (c.state == CreatureState::Fleeing) {
            c.pos += c.velocity * dt;
            // Bounding trot motion
            c.pos.y = std::max(0.0f, std::sin(time_ * 9.0f) * 0.28f);
        }
    }

    // 3. Update Fireflies / Golden Embers
    for (auto& f : fireflies_) {
        float t = time_ * f.speed + f.phase;
        f.pos.x = f.basePos.x + std::cos(t * 0.8f) * f.radius;
        f.pos.y = f.basePos.y + std::sin(t * 1.2f) * (f.radius * 0.6f);
        f.pos.z = f.basePos.z + std::sin(t * 0.9f) * (f.radius * 0.8f);
    }
}

void WildlifeSystem::draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
    // 1. Draw Crows / Ravens
    glUseProgram(wildlifeShader_);
    glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniform3fv(glGetUniformLocation(wildlifeShader_, "uCamPos"), 1, &camPos[0]);

    glm::vec3 crowColor(0.06f, 0.07f, 0.09f); // Sleek iridescent black plumage
    glUniform3fv(glGetUniformLocation(wildlifeShader_, "uColor"), 1, &crowColor[0]);

    for (const auto& b : birds_) {
        glm::mat4 mBody = glm::translate(glm::mat4(1.0f), b.pos);
        mBody = glm::rotate(mBody, glm::radians(b.yaw), glm::vec3(0, 1, 0));

        // Draw body
        glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uModel"), 1, GL_FALSE, &mBody[0][0]);
        glBindVertexArray(birdBodyVao_);
        glDrawArrays(GL_TRIANGLES, 0, birdBodyVerts_);

        // Wing flapping angle calculation
        float wingAngle = 0.0f;
        if (b.state == BirdState::Perched) {
            wingAngle = 6.0f + std::sin(time_ * 2.5f) * 3.0f; // resting folded
        } else {
            wingAngle = std::sin(b.flapTimer * 15.0f) * 38.0f; // rapid flapping
        }

        glBindVertexArray(birdWingVao_);

        // Right wing (+X)
        glm::mat4 mRightWing = glm::translate(mBody, glm::vec3(0.08f, 0.02f, 0.0f));
        mRightWing = glm::rotate(mRightWing, glm::radians(wingAngle), glm::vec3(0, 0, 1));
        glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uModel"), 1, GL_FALSE, &mRightWing[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, birdWingVerts_);

        // Left wing (-X, mirrored)
        glm::mat4 mLeftWing = glm::translate(mBody, glm::vec3(-0.08f, 0.02f, 0.0f));
        mLeftWing = glm::scale(mLeftWing, glm::vec3(-1.0f, 1.0f, 1.0f));
        mLeftWing = glm::rotate(mLeftWing, glm::radians(wingAngle), glm::vec3(0, 0, 1));
        glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uModel"), 1, GL_FALSE, &mLeftWing[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, birdWingVerts_);
    }

    // 2. Draw Forest Stags / Deer in the mist
    glm::vec3 stagColor(0.18f, 0.15f, 0.13f); // Earthy dark coat
    glUniform3fv(glGetUniformLocation(wildlifeShader_, "uColor"), 1, &stagColor[0]);
    glBindVertexArray(creatureVao_);

    for (const auto& c : creatures_) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), c.pos);
        m = glm::rotate(m, glm::radians(c.yaw), glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(c.scale));
        glUniformMatrix4fv(glGetUniformLocation(wildlifeShader_, "uModel"), 1, GL_FALSE, &m[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, creatureVerts_);
    }

    // 3. Draw Swarm of Bioluminescent Fireflies / Golden Embers
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive luminous blending
    glDisable(GL_CULL_FACE);

    glUseProgram(fireflyShader_);
    glUniformMatrix4fv(glGetUniformLocation(fireflyShader_, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(fireflyShader_, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniform3fv(glGetUniformLocation(fireflyShader_, "uCamPos"), 1, &camPos[0]);

    glBindVertexArray(fireflyVao_);
    for (const auto& f : fireflies_) {
        float pulse = 0.65f + 0.35f * std::sin(time_ * 3.5f + f.phase);
        glUniform1f(glGetUniformLocation(fireflyShader_, "uPulse"), pulse);

        // Billboard facing camera
        glm::mat4 m = glm::translate(glm::mat4(1.0f), f.pos);
        glUniformMatrix4fv(glGetUniformLocation(fireflyShader_, "uModel"), 1, GL_FALSE, &m[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, fireflyVerts_);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}
