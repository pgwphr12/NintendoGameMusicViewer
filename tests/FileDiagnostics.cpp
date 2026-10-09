// Local diagnostic runner; supplied music is never copied into release packages.
#include "core/IMusicBackend.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    try {
        for (int input = 1; input < argc; ++input) {
            auto backend = ngmv::makeBackend(ngmv::readMusicFile(argv[input]), argv[input]);
            std::cout << "FILE " << std::filesystem::path(argv[input]).filename().u8string()
                      << " tracks=" << backend->tracks().size()
                      << " voices=" << backend->channels().size() << '\n';
            for (size_t track = 0; track < backend->tracks().size(); ++track) {
                backend->selectTrack(int(track));
                ngmv::AudioBlock block;
                double mixEnergy = 0;
                std::vector<double> voiceEnergy(backend->channels().size());
                for (int i = 0; i < 563; ++i) { // six seconds at the fixed internal 48k rate
                    backend->render(512, block);
                    for (auto sample : block.mix) {
                        if (!std::isfinite(sample.l) || !std::isfinite(sample.r))
                            throw std::runtime_error("Non-finite PCM");
                        mixEnergy += sample.l * sample.l + sample.r * sample.r;
                    }
                    for (size_t c = 0; c < block.voices.size(); ++c)
                        for (auto sample : block.voices[c])
                            voiceEnergy[c] += sample.l * sample.l + sample.r * sample.r;
                }
                std::cout << "Track " << track + 1 << " mix_energy=" << mixEnergy;
                for (size_t c = 0; c < voiceEnergy.size(); ++c)
                    std::cout << " | " << backend->channels()[c].name << '=' << voiceEnergy[c];
                std::cout << '\n';
            }
            // Replay/seek and actual mute at the same sample position.
            backend->selectTrack(0);
            backend->seek(500);
            backend->muteMask((1u << backend->channels().size()) - 1);
            ngmv::AudioBlock muted;
            for (int i = 0; i < 100; ++i)
                backend->render(512, muted);
            double peak = 0;
            for (auto sample : muted.mix)
                peak = std::max(peak, double(std::max(std::abs(sample.l), std::abs(sample.r))));
            if (peak > .001)
                throw std::runtime_error("All-channel mute failed");
            std::cout << "Seek, replay and all-channel mute: PASS\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
