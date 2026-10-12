#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class AudioEngine {
public:
    static AudioEngine& instance();

    void init();
    void shutdown();

    void playBGM(const std::string& path, bool loop = true);
    void stopBGM();

    void playSFX(const std::string& channel, const std::string& path);
    void stopSFX(const std::string& channel);

    // Convenience game SFX triggers
    void playFootstep(bool running);
    void playWeaponSwing();
    void playFlashlight(bool turnOn);
    void playMonsterGrowl();
    void playMonsterRoar();
    void playStinger();
    void playHeartbeat(bool fast);
    void playBreathing(bool fast);
    void playMatchStrike();

    // Positional & psychological updates (heartbeat, whispers, growl frequency)
    void update(float dt, const glm::vec3& playerPos, const glm::vec3& nearestMonsterPos,
                float will = 1.0f, bool inDarkness = false);

private:
    AudioEngine() = default;
    ~AudioEngine();

    std::unordered_map<std::string, std::string> activeChannels_;
    float footstepCooldown_ = 0.0f;
    float monsterGrowlCooldown_ = 0.0f;
    float stingerCooldown_ = 0.0f;
    float heartbeatCooldown_ = 0.0f;
    float breathCooldown_ = 0.0f;
    bool bgmPlaying_ = false;
};
