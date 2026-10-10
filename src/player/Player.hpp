#pragma once
#include "audio/OutputLevel.hpp"
#include "audio/Reverb.hpp"
#include "audio/SdlOutput.hpp"
#include "channels/ChannelHistory.hpp"
#include "core/IMusicBackend.hpp"
#include <mutex>
#include <thread>
namespace ngmv {
class Player {
    StereoRing<> audio_;
    SdlOutput output_;
    OutputLevel level_;
    Reverb reverb_;
    std::atomic<bool> reverbEnabled_{false};
    std::atomic<float> reverbAmount_{.45f};
    std::atomic<float> reverbDecay_{1.2f};
    std::atomic<bool> dmcUsed_{false};
    std::atomic<uint32_t> usedChannels_{0};
    ChannelHistory history_;
    std::unique_ptr<IMusicBackend> backend_;
    std::thread worker_;
    std::atomic<bool> quit_{false};
    mutable std::mutex stateMutex_;
    std::vector<ChannelState> channelStates_;
    std::string error_;
    bool playing_ = false;
    int track_ = 0;
    uint64_t base_ = 0;
    std::atomic<bool> atEnd_{false};
    std::atomic<bool> repeat_{false};
    void haltWorker();
    void startWorker();
    void prefill();
    void produce();
    void restart(int track, int ms, bool play);

  public:
    Player() : output_(audio_) {}
    ~Player();
    void open(const std::filesystem::path &path);
    void select(int track);
    void play();
    void pause();
    void stop();
    void seek(int milliseconds);
    float outputGain() const {
        return level_.gain();
    }
    bool loaded() const {
        return bool(backend_);
    }
    bool playing() const {
        return playing_;
    }
    int track() const {
        return track_;
    }
    int rate() const {
        return output_.rate();
    }
    const std::vector<TrackInfo> &tracks() const;
    const std::vector<ChannelInfo> &channels() const;
    double seconds() const {
        return double(base_ + output_.consumed()) / InternalRate;
    }
    bool ended() const;
    void repeat(bool b) {
        repeat_ = b;
    }
    bool repeat() const {
        return repeat_;
    }
    void rate(int rate) {
        output_.changeRate(rate);
    }
    void master(float v) {
        output_.master(v);
    }
    bool dmcUsed() const {
        return dmcUsed_.load();
    }
    bool channelUsed(size_t index) const {
        return index < 32 && (usedChannels_.load() & (uint32_t(1) << index));
    }
    bool reverb() const {
        return reverbEnabled_.load();
    }
    void reverb(bool value) {
        reverbEnabled_ = value;
    }
    float reverbAmount() const {
        return reverbAmount_.load();
    }
    void reverbAmount(float value) {
        reverbAmount_ = std::clamp(value, 0.f, 1.f);
    }
    float reverbDecay() const {
        return reverbDecay_.load();
    }
    void reverbDecay(float value) {
        reverbDecay_ = std::clamp(value, .1f, 3.f);
    }
    float speed() const {
        return output_.speed();
    }
    void speed(float value) {
        output_.speed(value);
    }
    float master() const {
        return output_.master();
    }
    void channel(size_t index, ChannelState state);
    std::vector<ChannelState> states() const;
    std::vector<std::vector<float>> waveforms() const {
        return history_.snapshot(base_ + output_.consumed());
    }
    std::string error() const;
    uint64_t underruns() const {
        return output_.underruns();
    }
};
} // namespace ngmv
