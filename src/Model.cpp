#include "Model.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cstring>
#include <map>

namespace {

// Vertex layout: position(3) normal(3) uv(2)
Mesh uploadMesh(const std::vector<float>& verts, const std::vector<unsigned>& idx,
                GLuint tex, const glm::vec4& color) {
    Mesh m;
    m.indexCount = (GLsizei)idx.size();
    m.texture = tex;
    m.color = color;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    return m;
}

GLuint loadTexture(const cgltf_image* img, const std::string& baseDir,
                   std::map<const cgltf_image*, GLuint>& cache) {
    if (!img) return 0;
    auto it = cache.find(img);
    if (it != cache.end()) return it->second;

    int w = 0, h = 0, n = 0;
    unsigned char* px = nullptr;
    if (img->buffer_view) {
        const unsigned char* bytes =
            (const unsigned char*)img->buffer_view->buffer->data + img->buffer_view->offset;
        px = stbi_load_from_memory(bytes, (int)img->buffer_view->size, &w, &h, &n, 4);
    } else if (img->uri && std::strncmp(img->uri, "data:", 5) != 0) {
        std::string uri = img->uri;
        cgltf_decode_uri(&uri[0]);
        uri.resize(std::strlen(uri.c_str()));

        // Extract raw filename
        std::string filename = uri;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        // Candidate paths for Quaternius pack folder structures
        std::vector<std::string> candidates = {
            baseDir + uri,
            baseDir + filename,
            baseDir + "Textures/" + filename,
            baseDir + "../Textures/" + filename,
            baseDir + "../../Textures/" + filename,
            baseDir + "../" + filename,
            baseDir + "../../" + filename
        };

        for (const auto& candidate : candidates) {
            px = stbi_load(candidate.c_str(), &w, &h, &n, 4);
            if (px) break;
        }
    }
    if (!px) {
        std::fprintf(stderr, "Could not load texture '%s' for a material.\n", img->uri ? img->uri : "<buffer>");
        cache[img] = 0;
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(px);
    cache[img] = tex;
    return tex;
}

} // namespace

Model::~Model() { release(); }

Model::Model(Model&& other) noexcept : meshes_(std::move(other.meshes_)) {
    other.meshes_.clear();
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        release();
        meshes_ = std::move(other.meshes_);
        other.meshes_.clear();
    }
    return *this;
}

void Model::release() {
    for (auto& m : meshes_) {
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        // Textures are shared between meshes of one model; left to process exit for now.
    }
    meshes_.clear();
}

bool Model::loadGLB(const std::string& path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "Failed to parse %s\n", path.c_str());
        return false;
    }
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "Failed to load buffers for %s\n", path.c_str());
        cgltf_free(data);
        return false;
    }

    std::string baseDir;
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) baseDir = path.substr(0, slash + 1);

    std::map<const cgltf_image*, GLuint> texCache;

    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh) continue;

        float m[16];
        cgltf_node_transform_world(node, m);
        glm::mat4 world = glm::make_mat4(m);
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(world)));

        for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi) {
            const cgltf_primitive& prim = node->mesh->primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr;
            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                const cgltf_attribute& a = prim.attributes[ai];
                if (a.type == cgltf_attribute_type_position && a.index == 0) pos = a.data;
                else if (a.type == cgltf_attribute_type_normal && a.index == 0) nrm = a.data;
                else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv = a.data;
            }
            if (!pos) continue;

            std::vector<float> verts;
            verts.reserve(pos->count * 8);
            for (cgltf_size v = 0; v < pos->count; ++v) {
                float p[3] = {0, 0, 0}, n[3] = {0, 1, 0}, t[2] = {0, 0};
                cgltf_accessor_read_float(pos, v, p, 3);
                if (nrm) cgltf_accessor_read_float(nrm, v, n, 3);
                if (uv) cgltf_accessor_read_float(uv, v, t, 2);
                glm::vec3 wp = glm::vec3(world * glm::vec4(p[0], p[1], p[2], 1.0f));
                glm::vec3 wn = glm::normalize(normalMat * glm::vec3(n[0], n[1], n[2]));
                verts.insert(verts.end(), {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, t[0], t[1]});
            }

            std::vector<unsigned> idx;
            if (prim.indices) {
                idx.reserve(prim.indices->count);
                for (cgltf_size i = 0; i < prim.indices->count; ++i)
                    idx.push_back((unsigned)cgltf_accessor_read_index(prim.indices, i));
            } else {
                for (cgltf_size i = 0; i < pos->count; ++i) idx.push_back((unsigned)i);
            }

            glm::vec4 color(1.0f);
            GLuint tex = 0;
            if (prim.material && prim.material->has_pbr_metallic_roughness) {
                const auto& pbr = prim.material->pbr_metallic_roughness;
                color = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                  pbr.base_color_factor[2], pbr.base_color_factor[3]);
                if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image)
                    tex = loadTexture(pbr.base_color_texture.texture->image, baseDir, texCache);
            }
            meshes_.push_back(uploadMesh(verts, idx, tex, color));
        }
    }

    cgltf_free(data);
    std::printf("Loaded %s (%zu meshes)\n", path.c_str(), meshes_.size());
    return !meshes_.empty();
}

Model Model::box(const glm::vec3& size, const glm::vec4& color) {
    // Box with its base on y = 0, centered on x and z.
    struct Face { glm::vec3 n, u, v; };
    const Face faces[6] = {
        {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {{-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
        {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}, {{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}},
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}, {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},
    };
    std::vector<float> verts;
    std::vector<unsigned> idx;
    for (int f = 0; f < 6; ++f) {
        unsigned base = (unsigned)(verts.size() / 8);
        for (int c = 0; c < 4; ++c) {
            float su = (c == 1 || c == 2) ? 0.5f : -0.5f;
            float sv = (c >= 2) ? 0.5f : -0.5f;
            glm::vec3 p = (faces[f].n * 0.5f + faces[f].u * su + faces[f].v * sv) * size;
            p.y += size.y * 0.5f;
            verts.insert(verts.end(), {p.x, p.y, p.z, faces[f].n.x, faces[f].n.y, faces[f].n.z,
                                       su + 0.5f, sv + 0.5f});
        }
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    Model m;
    m.meshes_.push_back(uploadMesh(verts, idx, 0, color));
    return m;
}

Model Model::groundQuad(float h, const glm::vec4& color) {
    std::vector<float> verts = {
        -h, 0, -h, 0, 1, 0, 0, 0,
         h, 0, -h, 0, 1, 0, 1, 0,
         h, 0,  h, 0, 1, 0, 1, 1,
        -h, 0,  h, 0, 1, 0, 0, 1,
    };
    std::vector<unsigned> idx = {0, 2, 1, 0, 3, 2};
    Model m;
    m.meshes_.push_back(uploadMesh(verts, idx, 0, color));
    return m;
}

void Model::draw(GLuint program, const glm::mat4& modelMatrix) const {
    glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, &modelMatrix[0][0]);
    for (const auto& m : meshes_) {
        glUniform4fv(glGetUniformLocation(program, "uColor"), 1, &m.color[0]);
        glUniform1i(glGetUniformLocation(program, "uHasTex"), m.texture ? 1 : 0);
        if (m.texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m.texture);
        }
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}
