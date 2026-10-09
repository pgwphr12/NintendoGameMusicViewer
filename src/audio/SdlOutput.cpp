#include "SdlOutput.hpp"
#include <cstring>
#include <stdexcept>
namespace ngmv {
SdlOutput::SdlOutput(StereoRing<> &ring) : ring_(ring) {
    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        throw std::runtime_error(SDL_GetError());
    try {
        device_ = create(rate_);
        resampler_.rate(rate_);
    } catch (...) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw;
    }
}
SdlOutput::~SdlOutput() {
    if (device_)
        SDL_CloseAudioDevice(device_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
SDL_AudioDeviceID SdlOutput::create(int rate) {
    SDL_AudioSpec want{}, got{};
    want.freq = rate;
    want.channels = 2;
    want.format = AUDIO_F32SYS;
    want.samples = 1024;
    want.callback = callback;
    want.userdata = this;
    auto id = SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);
    if (!id)
        throw std::runtime_error(std::string("Audio device: ") + SDL_GetError());
    return id;
}
void SdlOutput::callback(void *self, Uint8 *out, int bytes) {
    auto &a = *static_cast<SdlOutput *>(self);
    std::memset(out, 0, bytes);
    a.resampler_.pull(a.ring_, reinterpret_cast<float *>(out), size_t(bytes) / sizeof(float) / 2,
                      a.master_.load(), a.speed_.load());
    a.consumed_.store(a.resampler_.consumed);
    a.underruns_.store(a.resampler_.underruns);
}
void SdlOutput::pause(bool value) {
    if (device_)
        SDL_PauseAudioDevice(device_, value ? 1 : 0);
    paused_ = value;
}
void SdlOutput::changeRate(int rate) {
    if (rate != 16000 && rate != 22050 && rate != 32000 && rate != 44100 && rate != 48000 &&
        rate != 96000)
        throw std::runtime_error("Choose 16000, 22050, 32000, 44100, 48000 or 96000 Hz.");
    if (rate == rate_)
        return;
    SDL_AudioDeviceID replacement = 0;
    bool wasPaused = paused_;
    try {
        replacement = create(rate);
    } catch (const std::exception &) {
        // Single-device drivers (including SDL's test driver) need a short reopen.
        // Keep the ring and fractional resampler clock; rollback if the new format fails.
        pause(true);
        SDL_CloseAudioDevice(device_);
        device_ = 0;
        try {
            replacement = create(rate);
        } catch (const std::exception &e) {
            std::string failure = e.what();
            try {
                device_ = create(rate_);
                pause(wasPaused);
            } catch (const std::exception &restore) {
                throw std::runtime_error(
                    failure + "; previous output could not be restored: " + restore.what());
            }
            throw std::runtime_error(failure + "; previous audio output restored.");
        }
    }
    if (device_) {
        pause(true);
        SDL_CloseAudioDevice(device_);
    }
    device_ = replacement;
    rate_ = rate;
    resampler_.rate(rate);
    pause(wasPaused);
}
void SdlOutput::reset() {
    if (!paused_)
        throw std::runtime_error("Audio reset requires a paused device.");
    resampler_.reset();
    consumed_ = 0;
    underruns_ = 0;
}
} // namespace ngmv
