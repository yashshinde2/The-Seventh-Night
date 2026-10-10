#include "SkinnedModel.h"

#include <cgltf.h>
#include <stb_image.h>

#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <map>
#include <cmath>

namespace {

struct SkinnedVertex {
    glm::vec3 pos;
    glm::vec3 nrm;
    glm::vec2 uv;
    glm::vec4 joints;
    glm::vec4 weights;
};

SkinnedMesh uploadSkinnedMesh(const std::vector<SkinnedVertex>& verts,
                              const std::vector<unsigned>& idx,
                              GLuint tex, const glm::vec4& color) {
    SkinnedMesh m;
    m.indexCount = (GLsizei)idx.size();
    m.texture = tex;
    m.color = color;

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SkinnedVertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned), idx.data(), GL_STATIC_DRAW);

    GLsizei stride = sizeof(SkinnedVertex);

    // 0: pos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, pos));

    // 1: nrm (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, nrm));

    // 2: uv (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, uv));

    // 3: joints (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, joints));

    // 4: weights (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(SkinnedVertex, weights));

    glBindVertexArray(0);
    return m;
}

GLuint loadSkinnedTexture(const cgltf_image* img, const std::string& baseDir,
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

        std::string filename = uri;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

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

SkinnedModel::~SkinnedModel() { release(); }

SkinnedModel::SkinnedModel(SkinnedModel&& other) noexcept
    : meshes_(std::move(other.meshes_)),
      nodes_(std::move(other.nodes_)),
      nameToNode_(std::move(other.nameToNode_)),
      joints_(std::move(other.joints_)),
      jointMatrices_(std::move(other.jointMatrices_)) {}

SkinnedModel& SkinnedModel::operator=(SkinnedModel&& other) noexcept {
    if (this != &other) {
        release();
        meshes_ = std::move(other.meshes_);
        nodes_ = std::move(other.nodes_);
        nameToNode_ = std::move(other.nameToNode_);
        joints_ = std::move(other.joints_);
        jointMatrices_ = std::move(other.jointMatrices_);
    }
    return *this;
}

void SkinnedModel::release() {
    for (auto& m : meshes_) {
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
    }
    meshes_.clear();
    nodes_.clear();
    joints_.clear();
    jointMatrices_.clear();
}

bool SkinnedModel::loadGLB(const std::string& path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "SkinnedModel parse failed: %s\n", path.c_str());
        return false;
    }
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "SkinnedModel buffer load failed: %s\n", path.c_str());
        cgltf_free(data);
        return false;
    }

    std::string baseDir;
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) baseDir = path.substr(0, slash + 1);

    std::map<const cgltf_image*, GLuint> texCache;

    // Build node hierarchy & preserve bind pose
    nodes_.resize(data->nodes_count);
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node* n = &data->nodes[i];
        SkeletonNode& sNode = nodes_[i];
        sNode.name = n->name ? n->name : ("Node_" + std::to_string(i));
        nameToNode_[sNode.name] = (int)i;

        glm::vec3 t(0.0f);
        glm::quat r(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 s(1.0f);

        if (n->has_translation) {
            t = glm::vec3(n->translation[0], n->translation[1], n->translation[2]);
        }
        if (n->has_rotation) {
            r = glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]);
        }
        if (n->has_scale) {
            s = glm::vec3(n->scale[0], n->scale[1], n->scale[2]);
        }

        sNode.bindPose = {t, r, s};
        sNode.currentTransform = sNode.bindPose;
    }

    // Connect node parent/children
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node* n = &data->nodes[i];
        for (cgltf_size c = 0; c < n->children_count; ++c) {
            int childIdx = (int)(n->children[c] - data->nodes);
            nodes_[i].children.push_back(childIdx);
            nodes_[childIdx].parent = (int)i;
        }
    }

    // Process Skin / Joints
    if (data->skins_count > 0) {
        const cgltf_skin& skin = data->skins[0];
        joints_.resize(skin.joints_count);
        jointMatrices_.resize(skin.joints_count, glm::mat4(1.0f));

        for (cgltf_size j = 0; j < skin.joints_count; ++j) {
            Joint& joint = joints_[j];
            int nodeIdx = (int)(skin.joints[j] - data->nodes);
            joint.nodeIndex = nodeIdx;
            joint.name = nodes_[nodeIdx].name;

            if (skin.inverse_bind_matrices) {
                float ibm[16];
                cgltf_accessor_read_float(skin.inverse_bind_matrices, j, ibm, 16);
                joint.inverseBindMatrix = glm::make_mat4(ibm);
            } else {
                joint.inverseBindMatrix = glm::mat4(1.0f);
            }
        }
    }

    // Process Mesh Primitives
    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh) continue;

        for (cgltf_size pi = 0; pi < node->mesh->primitives_count; ++pi) {
            const cgltf_primitive& prim = node->mesh->primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr;
            const cgltf_accessor *jnt = nullptr, *wgt = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
                const cgltf_attribute& a = prim.attributes[ai];
                if (a.type == cgltf_attribute_type_position && a.index == 0) pos = a.data;
                else if (a.type == cgltf_attribute_type_normal && a.index == 0) nrm = a.data;
                else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uv = a.data;
                else if (a.type == cgltf_attribute_type_joints && a.index == 0) jnt = a.data;
                else if (a.type == cgltf_attribute_type_weights && a.index == 0) wgt = a.data;
            }
            if (!pos) continue;

            std::vector<SkinnedVertex> verts(pos->count);
            for (cgltf_size v = 0; v < pos->count; ++v) {
                float p[3] = {0,0,0}, n[3] = {0,1,0}, t[2] = {0,0};
                float j[4] = {0,0,0,0}, w[4] = {1,0,0,0};

                cgltf_accessor_read_float(pos, v, p, 3);
                if (nrm) cgltf_accessor_read_float(nrm, v, n, 3);
                if (uv) cgltf_accessor_read_float(uv, v, t, 2);
                if (jnt) cgltf_accessor_read_float(jnt, v, j, 4);
                if (wgt) cgltf_accessor_read_float(wgt, v, w, 4);

                verts[v].pos = glm::vec3(p[0], p[1], p[2]);
                verts[v].nrm = glm::vec3(n[0], n[1], n[2]);
                verts[v].uv = glm::vec2(t[0], t[1]);
                verts[v].joints = glm::vec4(j[0], j[1], j[2], j[3]);
                verts[v].weights = glm::vec4(w[0], w[1], w[2], w[3]);
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
                if (pbr.base_color_texture.texture && pbr.base_color_texture.texture->image) {
                    tex = loadSkinnedTexture(pbr.base_color_texture.texture->image, baseDir, texCache);
                }
            }
            meshes_.push_back(uploadSkinnedMesh(verts, idx, tex, color));
        }
    }

    cgltf_free(data);
    std::printf("Loaded SkinnedModel %s (%zu meshes, %zu joints)\n", path.c_str(), meshes_.size(), joints_.size());

    // Apply bind pose initially
    applyPose(getBindPose());

    return !meshes_.empty();
}

Pose SkinnedModel::getBindPose() const {
    Pose p;
    p.nodeTransforms.resize(nodes_.size());
    for (size_t i = 0; i < nodes_.size(); ++i) {
        p.nodeTransforms[i] = nodes_[i].bindPose;
    }
    return p;
}

Pose SkinnedModel::samplePose(const AnimationClip* clip, float time, bool loop) const {
    if (!clip || nodes_.empty()) return getBindPose();

    float playbackTime = time;
    if (clip->duration > 0.0f) {
        playbackTime = loop ? std::fmod(time, clip->duration) : std::min(time, clip->duration);
    } else {
        playbackTime = 0.0f;
    }

    Pose p;
    p.nodeTransforms.resize(nodes_.size());

    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& node = nodes_[i];
        const AnimationChannel* channel = clip->getChannel(node.name);

        if (channel) {
            p.nodeTransforms[i].translation = channel->sampleTranslation(playbackTime, node.bindPose.translation);
            p.nodeTransforms[i].rotation = channel->sampleRotation(playbackTime, node.bindPose.rotation);
            p.nodeTransforms[i].scale = channel->sampleScale(playbackTime, node.bindPose.scale);
        } else {
            // Safe fallback to bind pose!
            p.nodeTransforms[i] = node.bindPose;
        }
    }
    return p;
}

Pose SkinnedModel::blendPoses(const Pose& a, const Pose& b, float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    size_t count = std::min(a.nodeTransforms.size(), b.nodeTransforms.size());

    Pose out;
    out.nodeTransforms.resize(count);

    for (size_t i = 0; i < count; ++i) {
        out.nodeTransforms[i].translation = glm::mix(a.nodeTransforms[i].translation,
                                                     b.nodeTransforms[i].translation, t);
        out.nodeTransforms[i].rotation = glm::normalize(glm::slerp(a.nodeTransforms[i].rotation,
                                                                   b.nodeTransforms[i].rotation, t));
        out.nodeTransforms[i].scale = glm::mix(a.nodeTransforms[i].scale,
                                               b.nodeTransforms[i].scale, t);
    }
    return out;
}

void SkinnedModel::computeGlobalTransforms(int nodeIdx, const glm::mat4& parentGlobal) {
    SkeletonNode& node = nodes_[nodeIdx];

    glm::mat4 T = glm::translate(glm::mat4(1.0f), node.currentTransform.translation);
    glm::mat4 R = glm::mat4_cast(node.currentTransform.rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), node.currentTransform.scale);

    glm::mat4 localMat = T * R * S;
    node.globalTransform = parentGlobal * localMat;

    for (int childIdx : node.children) {
        computeGlobalTransforms(childIdx, node.globalTransform);
    }
}

void SkinnedModel::applyPose(const Pose& pose) {
    size_t count = std::min(nodes_.size(), pose.nodeTransforms.size());
    for (size_t i = 0; i < count; ++i) {
        nodes_[i].currentTransform = pose.nodeTransforms[i];
    }

    // Traverse hierarchy from roots
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].parent == -1) {
            computeGlobalTransforms((int)i, glm::mat4(1.0f));
        }
    }

    // Compute joint matrices
    for (size_t j = 0; j < joints_.size(); ++j) {
        int nodeIdx = joints_[j].nodeIndex;
        if (nodeIdx >= 0 && nodeIdx < (int)nodes_.size()) {
            jointMatrices_[j] = nodes_[nodeIdx].globalTransform * joints_[j].inverseBindMatrix;
        }
    }
}

void SkinnedModel::updateSkeleton(const AnimationClip* clip, float time, bool loop) {
    Pose p = samplePose(clip, time, loop);
    applyPose(p);
}

glm::mat4 SkinnedModel::getBoneWorldTransform(const std::string& name, const glm::mat4& modelMatrix) const {
    auto it = nameToNode_.find(name);
    if (it != nameToNode_.end() && it->second >= 0 && it->second < (int)nodes_.size()) {
        return modelMatrix * nodes_[it->second].globalTransform;
    }
    return modelMatrix;
}

std::vector<std::string> SkinnedModel::getBoneNames() const {
    std::vector<std::string> names;
    names.reserve(joints_.size());
    for (const auto& j : joints_) {
        names.push_back(j.name);
    }
    return names;
}

void SkinnedModel::draw(GLuint program, const glm::mat4& modelMatrix) const {
    glUniformMatrix4fv(glGetUniformLocation(program, "uModel"), 1, GL_FALSE, &modelMatrix[0][0]);
    glUniform1i(glGetUniformLocation(program, "uIsSkinned"), 1);

    if (!jointMatrices_.empty()) {
        glUniformMatrix4fv(glGetUniformLocation(program, "uJointMatrices"),
                           (GLsizei)std::min(jointMatrices_.size(), (size_t)128),
                           GL_FALSE, glm::value_ptr(jointMatrices_[0]));
    }

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
