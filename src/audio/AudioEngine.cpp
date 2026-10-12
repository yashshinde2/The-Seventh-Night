#include "AudioEngine.h"
#include "../core/Assets.h"
#include <iostream>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace {
std::string resolveAudioPath(const std::string& path) {
    fs::path p(path);
    if (fs::exists(p)) return fs::absolute(p).string();
    fs::path root = AssetManager::findAssetsRoot();
    fs::path candidate = root / path;
    if (fs::exists(candidate)) return fs::absolute(candidate).string();
    return path;
}
}

AudioEngine& AudioEngine::instance() {
    static AudioEngine engine;
    return engine;
}

AudioEngine::~AudioEngine() {
    shutdown();
}

void AudioEngine::init() {
    shutdown();
}

void AudioEngine::shutdown() {
    stopBGM();
#ifdef _WIN32
    for (const auto& kv : activeChannels_) {
        std::string cmd = "close " + kv.first;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    }
    mciSendStringA("close all", NULL, 0, NULL);
#endif
    activeChannels_.clear();
}

void AudioEngine::playBGM(const std::string& path, bool loop) {
#ifdef _WIN32
    stopBGM();
    std::string fullPath = resolveAudioPath(path);
    if (!fs::exists(fullPath)) {
        std::fprintf(stderr, "[Audio] BGM not found: %s\n", fullPath.c_str());
        return;
    }
    std::string openCmd = "open \"" + fullPath + "\" alias bgm";
    MCIERROR err = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
    if (err == 0) {
        std::string playCmd = loop ? "play bgm repeat" : "play bgm from 0";
        mciSendStringA(playCmd.c_str(), NULL, 0, NULL);
        bgmPlaying_ = true;
        std::printf("[Audio] Ambient BGM started: %s\n", fullPath.c_str());
    } else {
        std::fprintf(stderr, "[Audio] Error opening BGM (code %lu): %s\n", err, fullPath.c_str());
    }
#endif
}

void AudioEngine::stopBGM() {
#ifdef _WIN32
    if (bgmPlaying_) {
        mciSendStringA("stop bgm", NULL, 0, NULL);
        mciSendStringA("close bgm", NULL, 0, NULL);
        bgmPlaying_ = false;
    }
#endif
}

void AudioEngine::playSFX(const std::string& channel, const std::string& path) {
#ifdef _WIN32
    std::string fullPath = resolveAudioPath(path);
    if (fullPath.empty() || !fs::exists(fullPath)) return;

    auto it = activeChannels_.find(channel);
    if (it != activeChannels_.end() && it->second == fullPath) {
        std::string cmd = "play " + channel + " from 0";
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    } else {
        if (it != activeChannels_.end()) {
            std::string closeCmd = "close " + channel;
            mciSendStringA(closeCmd.c_str(), NULL, 0, NULL);
        }
        std::string openCmd = "open \"" + fullPath + "\" alias " + channel;
        MCIERROR err = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
        if (err == 0) {
            std::string playCmd = "play " + channel + " from 0";
            mciSendStringA(playCmd.c_str(), NULL, 0, NULL);
            activeChannels_[channel] = fullPath;
        }
    }
#endif
}

void AudioEngine::stopSFX(const std::string& channel) {
#ifdef _WIN32
    auto it = activeChannels_.find(channel);
    if (it != activeChannels_.end()) {
        std::string cmd = "stop " + channel;
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    }
#endif
}

void AudioEngine::playFootstep(bool running) {
    if (footstepCooldown_ > 0.0f) return;
    footstepCooldown_ = running ? 0.32f : 0.52f;
    std::string sfx = running ?
        "Horror SFX Free/Character/Footsteps_ running.wav" :
        "Horror SFX Free/Character/Footsteps_walking.wav";
    playSFX("footstep", sfx);
}

void AudioEngine::playWeaponSwing() {
    playSFX("swing", "Horror SFX Free/Character/woosh.wav");
}

void AudioEngine::playFlashlight(bool turnOn) {
    std::string sfx = turnOn ?
        "Horror SFX Free/Character/Flashlight on.wav" :
        "Horror SFX Free/Character/Flashlight off.wav";
    playSFX("flashlight", sfx);
}

void AudioEngine::playMonsterGrowl() {
    playSFX("growl", "Horror SFX Free/Monsters & Ghosts/Monster_growl_1.wav");
}

void AudioEngine::playMonsterRoar() {
    playSFX("roar", "Horror SFX Free/Monsters & Ghosts/Monster_Roar_2.wav");
}

void AudioEngine::playStinger() {
    playSFX("stinger", "Horror SFX Free/Stingers and Spooky Triggers/Stinger.wav");
}

void AudioEngine::update(float dt, const glm::vec3& playerPos, const glm::vec3& nearestMonsterPos) {
    if (footstepCooldown_ > 0.0f) footstepCooldown_ -= dt;
    if (monsterGrowlCooldown_ > 0.0f) monsterGrowlCooldown_ -= dt;
    if (stingerCooldown_ > 0.0f) stingerCooldown_ -= dt;

    float dist = glm::distance(glm::vec2(playerPos.x, playerPos.z), glm::vec2(nearestMonsterPos.x, nearestMonsterPos.z));
    if (dist < 8.0f && stingerCooldown_ <= 0.0f) {
        playStinger();
        playMonsterRoar();
        stingerCooldown_ = 25.0f; // Don't spam stinger
        monsterGrowlCooldown_ = 6.0f;
    } else if (dist < 18.0f && monsterGrowlCooldown_ <= 0.0f) {
        playMonsterGrowl();
        monsterGrowlCooldown_ = 9.0f;
    }
}
