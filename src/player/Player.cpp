#include "Player.hpp"
#include <chrono>
#include <cmath>
#include <stdexcept>
namespace ngmv {
Player::~Player() {
    output_.pause(true);
    haltWorker();
}
const std::vector<TrackInfo> &Player::tracks() const {
    static std::vector<TrackInfo> empty;
    return backend_ ? backend_->tracks() : empty;
}
const std::vector<ChannelInfo> &Player::channels() const {
    static std::vector<ChannelInfo> empty;
    return backend_ ? backend_->channels() : empty;
}
void Player::haltWorker() {
    quit_ = true;
    if (worker_.joinable())
        worker_.join();
}
void Player::startWorker() {
    quit_ = false;
    worker_ = std::thread(&Player::produce, this);
}
void Player::prefill() {
    const size_t target = std::min(
        size_t(8192), std::max(size_t(4096), size_t(std::ceil(1024. * InternalRate /
                                                              output_.rate() * output_.speed())) +
                                                 1024));
    for (int i = 0; i < 500 && audio_.available() < target && !atEnd_ && error().empty(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (!error().empty())
        throw std::runtime_error(error());
}
void Player::open(const std::filesystem::path &path) {
    auto bytes = readMusicFile(path);
    auto format = identify(bytes);
    auto candidate = makeBackend(bytes, path);
    output_.pause(true);
    haltWorker();
    backend_ = std::move(candidate);
    bool fds = false;
    for (const auto &channel : backend_->channels())
        fds = fds || channel.name == "FDS WAVE";
    dmcUsed_ = false;
    level_.configure(format, fds);
    reverb_.reset();
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        channelStates_.assign(backend_->channels().size(), ChannelState{});
        error_.clear();
    }
    track_ = 0;
    base_ = 0;
    playing_ = false;
    atEnd_ = false;
    audio_.reset();
    output_.reset();
    history_.reset(channelStates_.size());
    startWorker();
    prefill();
}
void Player::restart(int track, int ms, bool play) {
    if (!backend_)
        return;
    output_.pause(true);
    playing_ = false;
    haltWorker();
    try {
        level_.reset();
        reverb_.reset();
        if (track != track_)
            dmcUsed_ = false;
        backend_->selectTrack(track);
        if (ms)
            backend_->seek(ms);
        track_ = track;
        base_ = uint64_t(ms) * InternalRate / 1000;
        audio_.reset();
        output_.reset();
        history_.reset(backend_->channels().size(), base_);
        atEnd_ = false;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            error_.clear();
        }
        startWorker();
        prefill();
        if (play)
            this->play();
    } catch (...) {
        playing_ = false;
        throw;
    }
}
void Player::select(int track) {
    if (!backend_ || track < 0 || track >= int(tracks().size()))
        return;
    restart(track, 0, playing_);
}
void Player::play() {
    if (!backend_)
        return;
    if (ended()) {
        restart(track_, 0, false);
    }
    prefill();
    output_.pause(false);
    playing_ = true;
}
void Player::pause() {
    output_.pause(true);
    playing_ = false;
}
void Player::stop() {
    restart(track_, 0, false);
}
void Player::seek(int ms) {
    if (!backend_)
        return;
    int length = tracks()[track_].lengthMs;
    if (length > 0)
        ms = std::min(ms, length - 1);
    restart(track_, std::max(0, ms), playing_);
}
void Player::channel(size_t i, ChannelState state) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (i < channelStates_.size()) {
        state.volume = std::clamp(state.volume, 0.f, 2.f);
        channelStates_[i] = state;
    }
}
std::vector<ChannelState> Player::states() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return channelStates_;
}
std::string Player::error() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return error_;
}
bool Player::ended() const {
    return atEnd_ && audio_.available() <= 2;
}
void Player::produce() {
    try {
        AudioBlock block;
        uint64_t produced = base_;
        int length = tracks()[track_].lengthMs;
        while (!quit_) {
            if (audio_.available() > 8192 - 512) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            if (length > 0 && produced >= uint64_t(length) * InternalRate / 1000) {
                atEnd_ = true;
                break;
            }
            size_t frames = 512;
            if (length > 0)
                frames = size_t(
                    std::min(uint64_t(frames), uint64_t(length) * InternalRate / 1000 - produced));
            auto state = states();
            uint32_t mask = 0;
            bool unity = true;
            for (size_t i = 0; i < state.size(); i++) {
                if (state[i].mute)
                    mask |= uint32_t(1) << i;
                if (state[i].volume != 1)
                    unity = false;
            }
            backend_->muteMask(mask);
            backend_->render(frames, block);
            history_.append(block);
            if (!dmcUsed_)
                for (size_t c = 0; c < backend_->channels().size(); ++c)
                    if (backend_->channels()[c].name == "DMC")
                        for (auto sample : block.voices[c])
                            if (std::max(std::abs(sample.l), std::abs(sample.r)) > .0001f) {
                                dmcUsed_ = true;
                                break;
                            }
            if (!unity)
                for (size_t f = 0; f < frames; f++) {
                    Stereo sample;
                    for (size_t c = 0; c < state.size(); c++)
                        if (!state[c].mute) {
                            sample.l += block.voices[c][f].l * state[c].volume;
                            sample.r += block.voices[c][f].r * state[c].volume;
                        }
                    block.mix[f] = sample;
                }
            reverb_.enabled(reverbEnabled_.load());
            reverb_.amount(reverbAmount_.load());
            reverb_.decay(reverbDecay_.load());
            for (auto &sample : block.mix)
                sample = level_.process(reverb_.process(sample));
            if (!audio_.push(block.mix.data(), frames))
                throw std::runtime_error("Audio ring producer overflow.");
            produced += frames;
        }
    } catch (const std::exception &e) {
        std::lock_guard<std::mutex> lock(stateMutex_);
        error_ = e.what();
        atEnd_ = true;
    }
}
} // namespace ngmv
