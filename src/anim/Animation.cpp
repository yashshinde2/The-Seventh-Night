#include "Animation.h"

#include <cgltf.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cctype>

namespace {

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

static bool istarts_with(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower((unsigned char)str[i]) != std::tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

static bool icontains(const std::string& haystack, const std::string& needle) {
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::tolower((unsigned char)ch1) == std::tolower((unsigned char)ch2); }
    );
    return (it != haystack.end());
}

} // namespace

glm::vec3 AnimationChannel::sampleTranslation(float time, const glm::vec3& fallback) const {
    if (translationKeys.empty()) return fallback;
    if (translationKeys.size() == 1 || time <= translationKeys.front().time)
        return translationKeys.front().value;
    if (time >= translationKeys.back().time)
        return translationKeys.back().value;

    // Binary search with std::upper_bound
    auto it = std::upper_bound(
        translationKeys.begin(), translationKeys.end(), time,
        [](float t, const KeyframeVec3& k) { return t < k.time; }
    );
    size_t idx1 = std::distance(translationKeys.begin(), it);
    size_t idx0 = idx1 - 1;

    if (interp == AnimInterpolation::Step) {
        return translationKeys[idx0].value;
    }

    float t0 = translationKeys[idx0].time;
    float t1 = translationKeys[idx1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::mix(translationKeys[idx0].value, translationKeys[idx1].value, factor);
}

glm::quat AnimationChannel::sampleRotation(float time, const glm::quat& fallback) const {
    if (rotationKeys.empty()) return fallback;
    if (rotationKeys.size() == 1 || time <= rotationKeys.front().time)
        return rotationKeys.front().value;
    if (time >= rotationKeys.back().time)
        return rotationKeys.back().value;

    // Binary search with std::upper_bound
    auto it = std::upper_bound(
        rotationKeys.begin(), rotationKeys.end(), time,
        [](float t, const KeyframeQuat& k) { return t < k.time; }
    );
    size_t idx1 = std::distance(rotationKeys.begin(), it);
    size_t idx0 = idx1 - 1;

    if (interp == AnimInterpolation::Step) {
        return rotationKeys[idx0].value;
    }

    float t0 = rotationKeys[idx0].time;
    float t1 = rotationKeys[idx1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::slerp(rotationKeys[idx0].value, rotationKeys[idx1].value, factor);
}

glm::vec3 AnimationChannel::sampleScale(float time, const glm::vec3& fallback) const {
    if (scaleKeys.empty()) return fallback;
    if (scaleKeys.size() == 1 || time <= scaleKeys.front().time)
        return scaleKeys.front().value;
    if (time >= scaleKeys.back().time)
        return scaleKeys.back().value;

    // Binary search with std::upper_bound
    auto it = std::upper_bound(
        scaleKeys.begin(), scaleKeys.end(), time,
        [](float t, const KeyframeVec3& k) { return t < k.time; }
    );
    size_t idx1 = std::distance(scaleKeys.begin(), it);
    size_t idx0 = idx1 - 1;

    if (interp == AnimInterpolation::Step) {
        return scaleKeys[idx0].value;
    }

    float t0 = scaleKeys[idx0].time;
    float t1 = scaleKeys[idx1].time;
    float factor = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
    return glm::mix(scaleKeys[idx0].value, scaleKeys[idx1].value, factor);
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

void AnimationClip::stripRootMotion(const std::string& rootBoneName) {
    // Find target bone (exact or fallback to "Root" / "Hips")
    AnimationChannel* target = nullptr;
    auto it = nodeToChannel.find(rootBoneName);
    if (it != nodeToChannel.end()) {
        target = &channels[it->second];
    } else {
        for (auto& chan : channels) {
            if (icontains(chan.targetNodeName, "root") || icontains(chan.targetNodeName, "hips")) {
                target = &chan;
                break;
            }
        }
    }

    if (target && !target->translationKeys.empty()) {
        for (auto& key : target->translationKeys) {
            key.value.x = 0.0f; // Keep physics responsible for horizontal translation
            key.value.z = 0.0f;
        }
        std::printf("[Anim] Stripped root motion X/Z on clip '%s' (bone: %s)\n",
                    name.c_str(), target->targetNodeName.c_str());
    }
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

            // Interpolation mode
            bool isCubicSpline = false;
            if (sampler->interpolation == cgltf_interpolation_type_step) {
                channel.interp = AnimInterpolation::Step;
            } else if (sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
                channel.interp = AnimInterpolation::CubicSpline;
                isCubicSpline = true;
            } else {
                channel.interp = AnimInterpolation::Linear;
            }

            cgltf_size timeCount = sampler->input->count;
            std::vector<float> times(timeCount);
            for (cgltf_size t = 0; t < timeCount; ++t) {
                cgltf_accessor_read_float(sampler->input, t, &times[t], 1);
                if (times[t] > clip.duration) clip.duration = times[t];
            }

            if (chan.target_path == cgltf_animation_path_type_translation) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[3];
                    cgltf_size outIdx = isCubicSpline ? (3 * t + 1) : t;
                    cgltf_accessor_read_float(sampler->output, outIdx, v, 3);
                    channel.translationKeys.push_back({times[t], glm::vec3(v[0], v[1], v[2])});
                }
            } else if (chan.target_path == cgltf_animation_path_type_rotation) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[4];
                    cgltf_size outIdx = isCubicSpline ? (3 * t + 1) : t;
                    cgltf_accessor_read_float(sampler->output, outIdx, v, 4);
                    channel.rotationKeys.push_back({times[t], glm::quat(v[3], v[0], v[1], v[2])});
                }
            } else if (chan.target_path == cgltf_animation_path_type_scale) {
                for (cgltf_size t = 0; t < timeCount; ++t) {
                    float v[3];
                    cgltf_size outIdx = isCubicSpline ? (3 * t + 1) : t;
                    cgltf_accessor_read_float(sampler->output, outIdx, v, 3);
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

    for (const auto& clip : clips) {
        if (iequals(clip.name, name)) return &clip;
    }
    for (const auto& clip : clips) {
        if (icontains(clip.name, name)) return &clip;
    }
    return nullptr;
}

const AnimationClip* AnimationLibrary::findClipAny(const std::vector<std::string>& candidateNames, const std::string& roleDesc) const {
    // Pass 1: Exact match (case-insensitive)
    for (const auto& candidate : candidateNames) {
        for (const auto& clip : clips) {
            if (iequals(clip.name, candidate)) {
                if (!roleDesc.empty()) {
                    std::printf("[Anim] Role '%s' mapped (Exact): '%s'\n", roleDesc.c_str(), clip.name.c_str());
                }
                return &clip;
            }
        }
    }

    // Pass 2: Starts with candidate name
    for (const auto& candidate : candidateNames) {
        for (const auto& clip : clips) {
            if (istarts_with(clip.name, candidate)) {
                // Ignore Zombie / Carry unless candidate explicitly contains it
                if (!icontains(candidate, "zombie") && icontains(clip.name, "zombie")) continue;
                if (!icontains(candidate, "carry") && icontains(clip.name, "carry")) continue;

                if (!roleDesc.empty()) {
                    std::printf("[Anim] Role '%s' mapped (Prefix): '%s'\n", roleDesc.c_str(), clip.name.c_str());
                }
                return &clip;
            }
        }
    }

    // Pass 3: Substring contains candidate name
    for (const auto& candidate : candidateNames) {
        for (const auto& clip : clips) {
            if (icontains(clip.name, candidate)) {
                if (!icontains(candidate, "zombie") && icontains(clip.name, "zombie")) continue;
                if (!icontains(candidate, "carry") && icontains(clip.name, "carry")) continue;

                if (!roleDesc.empty()) {
                    std::printf("[Anim] Role '%s' mapped (Substring): '%s'\n", roleDesc.c_str(), clip.name.c_str());
                }
                return &clip;
            }
        }
    }

    if (!roleDesc.empty()) {
        std::printf("[Anim WARNING] No clip found for role '%s'\n", roleDesc.c_str());
    }
    return nullptr;
}

void AnimationLibrary::printClipNames() const {
    std::printf("--- Animation Clip List (%zu clips) ---\n", clips.size());
    for (size_t i = 0; i < clips.size(); ++i) {
        std::printf(" [%zu] '%s' (duration: %.2fs)\n", i, clips[i].name.c_str(), clips[i].duration);
    }
    std::fflush(stdout);
}

float AnimationLibrary::checkRetarget(const AnimationClip& clip, const std::vector<std::string>& skeletonBoneNames) const {
    if (clip.channels.empty() || skeletonBoneNames.empty()) return 0.0f;
    size_t matched = 0;
    for (const auto& chan : clip.channels) {
        for (const auto& bone : skeletonBoneNames) {
            if (iequals(chan.targetNodeName, bone)) {
                ++matched;
                break;
            }
        }
    }
    float pct = (100.0f * (float)matched) / (float)clip.channels.size();
    std::printf("[Anim Retarget] Clip '%s': %zu/%zu channels matched (%.1f%%)\n",
                clip.name.c_str(), matched, clip.channels.size(), pct);
    if (pct < 90.0f) {
        std::printf("[Anim Retarget WARNING] Clip '%s' has low match percentage (%.1f%% < 90%%)!\n",
                    clip.name.c_str(), pct);
    }
    return pct;
}
