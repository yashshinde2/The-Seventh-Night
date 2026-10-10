#include "Animation.h"

#include <cgltf.h>
#include <algorithm>
#include <cstdio>
#include <cmath>

glm::vec3 AnimationChannel::sampleTranslation(float time) const {
    if (translationKeys.empty()) return glm::vec3(0.0f);
    if (translationKeys.size() == 1 || time <= translationKeys.front().time)
        return translationKeys.front().value;
    if (time >= translationKeys.back().time)
        return translationKeys.back().value;

    size_t idx = 0;
    for (size_t i = 0; i < translationKeys.size() - 1; ++i) {
        if (time >= translationKeys[i].time && time <= translationKeys[i + 1].time) {
            idx = i;
            break;
        }
    }
    float t0 = translationKeys[idx].time;
    float t1 = translationKeys[idx + 1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::mix(translationKeys[idx].value, translationKeys[idx + 1].value, factor);
}

glm::quat AnimationChannel::sampleRotation(float time) const {
    if (rotationKeys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (rotationKeys.size() == 1 || time <= rotationKeys.front().time)
        return rotationKeys.front().value;
    if (time >= rotationKeys.back().time)
        return rotationKeys.back().value;

    size_t idx = 0;
    for (size_t i = 0; i < rotationKeys.size() - 1; ++i) {
        if (time >= rotationKeys[i].time && time <= rotationKeys[i + 1].time) {
            idx = i;
            break;
        }
    }
    float t0 = rotationKeys[idx].time;
    float t1 = rotationKeys[idx + 1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::slerp(rotationKeys[idx].value, rotationKeys[idx + 1].value, factor);
}

glm::vec3 AnimationChannel::sampleScale(float time) const {
    if (scaleKeys.empty()) return glm::vec3(1.0f);
    if (scaleKeys.size() == 1 || time <= scaleKeys.front().time)
        return scaleKeys.front().value;
    if (time >= scaleKeys.back().time)
        return scaleKeys.back().value;

    size_t idx = 0;
    for (size_t i = 0; i < scaleKeys.size() - 1; ++i) {
        if (time >= scaleKeys[i].time && time <= scaleKeys[i + 1].time) {
            idx = i;
            break;
        }
    }
    float t0 = scaleKeys[idx].time;
    float t1 = scaleKeys[idx + 1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::mix(scaleKeys[idx].value, scaleKeys[idx + 1].value, factor);
}

void AnimationClip::buildChannelMap() {
    nodeToChannel.clear();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (!channels[i].targetNodeName.empty()) {
            nodeToChannel[channels[i].targetNodeName] = i;
        }
    }
}

const AnimationChannel* AnimationClip::getChannel(const std::string& nodeName) const {
    auto it = nodeToChannel.find(nodeName);
    if (it != nodeToChannel.end()) {
        return &channels[it->second];
    }
    return nullptr;
}

bool AnimationLibrary::loadGLBAnimations(const std::string& glbPath) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, glbPath.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "Animation parse failed: %s\n", glbPath.c_str());
        return false;
    }
    if (cgltf_load_buffers(&options, data, glbPath.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "Animation buffer load failed: %s\n", glbPath.c_str());
        cgltf_free(data);
        return false;
    }

    for (cgltf_size i = 0; i < data->animations_count; ++i) {
        const cgltf_animation& anim = data->animations[i];
        AnimationClip clip;
        clip.name = anim.name ? anim.name : ("Clip_" + std::to_string(clips.size()));

        std::unordered_map<const cgltf_node*, size_t> nodeChannelIndex;

        for (cgltf_size c = 0; c < anim.channels_count; ++c) {
            const cgltf_animation_channel& chan = anim.channels[c];
            if (!chan.target_node || !chan.target_node->name) continue;

            const cgltf_node* targetNode = chan.target_node;
            std::string nodeName = targetNode->name;

            size_t chanIdx = 0;
            auto it = nodeChannelIndex.find(targetNode);
            if (it == nodeChannelIndex.end()) {
                AnimationChannel newChan;
                newChan.targetNodeName = nodeName;
                clip.channels.push_back(newChan);
                chanIdx = clip.channels.size() - 1;
                nodeChannelIndex[targetNode] = chanIdx;
            } else {
                chanIdx = it->second;
            }

            AnimationChannel& channel = clip.channels[chanIdx];
            const cgltf_animation_sampler* sampler = chan.sampler;
            if (!sampler || !sampler->input || !sampler->output) continue;

            cgltf_size timeCount = sampler->input->count;
            std::vector<float> times(timeCount);
            for (cgltf_size t = 0; t < timeCount; ++t) {
                cgltf_accessor_read_float(sampler->input, t, &times[t], 1);
                if (times[t] > clip.duration) clip.duration = times[t];
            }

            if (chan.target_property == cgltf_animation_path_type_translation) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[3];
                    cgltf_accessor_read_float(sampler->output, t, v, 3);
                    channel.translationKeys.push_back({times[t], glm::vec3(v[0], v[1], v[2])});
                }
            } else if (chan.target_property == cgltf_animation_path_type_rotation) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[4];
                    cgltf_accessor_read_float(sampler->output, t, v, 4);
                    // glTF quaternion order: x, y, z, w -> glm::quat(w, x, y, z)
                    channel.rotationKeys.push_back({times[t], glm::quat(v[3], v[0], v[1], v[2])});
                }
            } else if (chan.target_property == cgltf_animation_path_type_scale) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[3];
                    cgltf_accessor_read_float(sampler->output, t, v, 3);
                    channel.scaleKeys.push_back({times[t], glm::vec3(v[0], v[1], v[2])});
                }
            }
        }
        clip.buildChannelMap();
        clipMap[clip.name] = clips.size();
        clips.push_back(clip);
    }

    cgltf_free(data);
    std::printf("Loaded %zu animation clips from %s\n", clips.size(), glbPath.c_str());
    return !clips.empty();
}

const AnimationClip* AnimationLibrary::findClip(const std::string& name) const {
    auto it = clipMap.find(name);
    if (it != clipMap.end()) return &clips[it->second];

    // Case-insensitive substring match fallback
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    for (const auto& clip : clips) {
        std::string clipLower = clip.name;
        std::transform(clipLower.begin(), clipLower.end(), clipLower.begin(), ::tolower);
        if (clipLower.find(lowerName) != std::string::npos) return &clip;
    }
    return nullptr;
}

void AnimationLibrary::printClipNames() const {
    std::printf("--- Animation Clip List (%zu clips) ---\n", clips.size());
    for (size_t i = 0; i < clips.size(); ++i) {
        std::printf(" [%zu] '%s' (duration: %.2fs)\n", i, clips[i].name.c_str(), clips[i].duration);
    }
}
