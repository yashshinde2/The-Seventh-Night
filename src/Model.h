#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// One drawable chunk of geometry (one glTF primitive).
struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    GLuint texture = 0;               // 0 = no texture
    glm::vec4 color{1.0f};            // base color factor
};

// A static model. World transforms inside the glTF file are baked into the
// vertices, so a whole tree or rock loads as one object.
class Model {
public:
    Model() = default;
    ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    // Loads a .glb or .gltf file (Quaternius packs ship both).
    bool loadGLB(const std::string& path);

    // Simple built-in shapes, used as placeholders (player, ground).
    static Model box(const glm::vec3& size, const glm::vec4& color);
    static Model groundQuad(float halfSize, const glm::vec4& color);

    void draw(GLuint program, const glm::mat4& modelMatrix) const;

private:
    void release();
    std::vector<Mesh> meshes_;
};
