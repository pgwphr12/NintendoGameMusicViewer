#include "audio/Resampler.hpp"
#include "formats/UsfVoiceTap.hpp"
extern "C" {
#include <rsp_hle/alist.h>
#include <rsp_hle/hle.h>
}
#include "core/IMusicBackend.hpp"
#include "player/FolderPlaylist.hpp"
#include "player/Player.hpp"
#include "visualizer/Envelope.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ngmv;
static void require(bool yes, const char *message) {
    if (!yes)
        throw std::runtime_error(message);
}
static double energy(const std::vector<Stereo> &s) {
    double e = 0;
    for (auto v : s)
        e += v.l * v.l + v.r * v.r;
    return e / std::max(size_t(1), s.size());
}
static void newAudioTests(const std::filesystem::path &root) {
    auto gba = makeBackend(readMusicFile(root / "pcm.gsf"), root / "pcm.gsf");
    AudioBlock pcm;
    gba->render(8192, pcm);
    require(energy(pcm.voices[4]) > .001 && energy(pcm.voices[5]) > .001,
            "Independent timer-driven GBA PCM FIFO disappeared");
    double difference = 0;
    for (size_t i = 0; i < pcm.mix.size(); ++i)
        difference += std::abs(pcm.voices[4][i].l - pcm.voices[5][i].l);
    require(difference > 10, "GBA PCM streams became copies");
    gba->muteMask(0x3f);
    gba->render(8192, pcm);
    gba->render(8192, pcm);
    require(energy(pcm.mix) < 1e-7 && energy(pcm.voices[4]) > .001,
            "GBA PCM mute lost source waveform");
    auto stream = makeBackend(readMusicFile(root / "stems.bcstm"), root / "stems.bcstm");
    stream->render(4096, pcm);
    require(pcm.voices.size() == 5 && energy(pcm.voices[4]) > .001, "3DS stored stems disappeared");
    for (auto v : pcm.voices[4])
        require(v.l == v.r, "3DS mono stem was routed to one side");
    require(pcm.voices[0][100].r == 0 && pcm.voices[1][100].l == 0,
            "3DS stereo stem lost its track-table routing");

    UsfVoiceTap tap;
    hle_t hle{};
    std::vector<uint8_t> dram(8192);
    hle.dram = dram.data();
    hle.viewer = &tap.callbacks;
    auto *samples = reinterpret_cast<int16_t *>(hle.alist_buffer);
    int16_t vol[] = {16384, 8192};
    int32_t rate[] = {65536, 65536};
    alist_clear(&hle, 512, 128);
    for (int voice = 0; voice < 2; ++voice) {
        for (int i = 0; i < 16; ++i)
            samples[i ^ 1] = int16_t(12000 * std::sin(i * (voice ? .7 : .3)));
        alist_envmix_exp(&hle, true, false, 512, 576, 640, 704, 0, 32, 32767, 0, vol, vol, rate,
                         128 + voice * 128);
    }
    alist_interleave(&hle, 1024, 512, 576, 32);
    alist_save(&hle, 1024, 4096, 64);
    tap.callbacks.dma(&tap, 4096, reinterpret_cast<int16_t *>(dram.data() + 4096), 16);
    require(tap.used == 2 && tap.pending.size() == 16, "N64 HLE slots/DMA clock lost");
    difference = 0;
    for (const auto &frame : tap.pending) {
        require(std::abs(frame.back().l) < .0001f && std::abs(frame.back().r) < .0001f,
                "N64 dry voices do not reconstruct the real interleaved DMA output");
        difference += std::abs(frame[1].l - frame[2].l);
    }
    require(difference > .1, "N64 source slots became copies");
    tap.pending.clear();
    alist_clear(&hle, 512, 128);
    alist_interleave(&hle, 1024, 512, 576, 32);
    alist_save(&hle, 1024, 4096, 64);
    tap.callbacks.dma(&tap, 4096, reinterpret_cast<int16_t *>(dram.data() + 4096), 16);
    for (const auto &frame : tap.pending)
        require(frame[1].l == 0 && frame[2].r == 0, "N64 shadow retained stale voice samples");

    StereoRing<> ring;
    std::vector<Stereo> input(12000);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = {float(.5 * std::sin(i * 2 * 3.141592653589793 * 20000 / 48000)), 0};
    ring.push(input.data(), input.size());
    OutputResampler resampler;
    resampler.rate(16000);
    std::vector<float> output(6000);
    resampler.pull(ring, output.data(), 3000, 1);
    double alias = 0;
    for (size_t i = 100; i < 2900; ++i)
        alias += output[2 * i] * output[2 * i];
    require(alias / 2800 < .00001 && resampler.underruns == 0,
            "Sinc output resampling failed to suppress downsampling aliases");
    std::cout
        << "GBA Direct Sound, N64 HLE envelope/interleave/save/DMA and sinc anti-alias: PASS\n";
}
static void wave(const std::filesystem::path &file, const std::vector<Stereo> &pcm) {
    std::ofstream f(file, std::ios::binary);
    uint32_t bytes = uint32_t(pcm.size() * 4), riff = bytes + 36, fmt = 16, rate = 48000,
             byteRate = 192000;
    uint16_t one = 1, two = 2, align = 4, bits = 16;
    f.write("RIFF", 4);
    f.write((char *)&riff, 4);
    f.write("WAVEfmt ", 8);
    f.write((char *)&fmt, 4);
    f.write((char *)&one, 2);
    f.write((char *)&two, 2);
    f.write((char *)&rate, 4);
    f.write((char *)&byteRate, 4);
    f.write((char *)&align, 2);
    f.write((char *)&bits, 2);
    f.write("data", 4);
    f.write((char *)&bytes, 4);
    for (auto v : pcm) {
        short a = short(std::clamp(v.l, -1.f, 1.f) * 32767),
              b = short(std::clamp(v.r, -1.f, 1.f) * 32767);
        f.write((char *)&a, 2);
        f.write((char *)&b, 2);
    }
}
static void formatTests(const std::filesystem::path &root) {
    for (auto pair : {std::make_pair("channels.nsf", 5),
                      {"channels.nsfe", 5},
                      {"channels.gbs", 4},
                      {"channels.spc", 8},
                      {"fds.nsf", 6},
                      {"fds-banked.nsf", 6},
                      {"fds-banked.nsfe", 6}}) {
        auto backend = makeBackend(readMusicFile(root / pair.first));
        require(backend->channels().size() == size_t(pair.second), "Wrong hardware voice count");
        AudioBlock out;
        std::vector<Stereo> pcm;
        double total = 0;
        for (int i = 0; i < 94; i++) {
            backend->render(512, out);
            total += energy(out.mix);
            pcm.insert(pcm.end(), out.mix.begin(), out.mix.end());
            require(out.voices.size() == size_t(pair.second), "Missing channel buffers");
        }
        std::cout << pair.first << " voices=" << pair.second << " energy=" << total << "\n";
        require(total > 1e-5, "Real format produced silence");
        require(energy(out.voices[0]) > 1e-7, "First voice is silent");
        require(energy(out.voices[1]) > 1e-7, "Second voice is silent");
        double diff = 0;
        for (size_t j = 0; j < out.voices[0].size(); j++)
            diff += std::abs(out.voices[0][j].l - out.voices[1][j].l);
        require(diff > .01, "Channel waveforms are copies");
        if (pair.second == 5) {
            require(backend->channels()[0].name == "PULSE 1", "Wrong NES labels");
            require(energy(out.voices[3]) < 1e-8, "Inactive noise is not silent");
            backend->selectTrack(1);
            for (int i = 0; i < 30; i++)
                backend->render(512, out);
            require(energy(out.voices[0]) < 1e-8 && energy(out.voices[1]) > 1e-7,
                    "Track switch did not change real voices");
        }
        if (pair.second == 6)
            require(energy(out.voices[5]) > 1e-7 && backend->channels()[5].name == "FDS WAVE",
                    "FDS waveform unavailable");
        wave(root / (std::string(pair.first) + ".wav"), pcm);
    }
    for (const char *name : {"empty.nsf", "invalid.nsf"}) {
        bool rejected = false;
        try {
            makeBackend(readMusicFile(root / name));
        } catch (...) {
            rejected = true;
        }
        require(rejected, "Invalid file accepted");
    }
    auto bytes = readMusicFile(root / "channels.nsf");
    bytes.resize(20);
    bool rejected = false;
    try {
        makeBackend(bytes);
    } catch (...) {
        rejected = true;
    }
    require(rejected, "Truncated NSF accepted");
    auto metadata = makeBackend(readMusicFile(root / "channels.nsfe"));
    require(metadata->tracks().size() == 2 && metadata->tracks()[0].title == "Both pulses" &&
                metadata->tracks()[0].lengthMs == 1000,
            "NSFE metadata missing");
    auto first = makeBackend(readMusicFile(root / "channels.nsf"));
    auto second = makeBackend(readMusicFile(root / "channels.nsf"));
    second->muteMask(1);
    AudioBlock a, b;
    for (int i = 0; i < 40; i++) {
        first->render(512, a);
        second->render(512, b);
    }
    require(energy(a.mix) > energy(b.mix), "Mute did not change actual mix");
    require(a.voices[0][100].l == b.voices[0][100].l,
            "Mute incorrectly modified independent pre-fader tap");
    std::cout << "Format opening, metadata, track switching, actual audio, FDS, hardware names, "
                 "mute and independent channel buffers: PASS\n";
}
static void psfTests(const std::filesystem::path &root) {
    for (auto name : {"channels.gsf", "channels.minigsf", "channels.2sf", "channels.mini2sf"}) {
        auto file = root / name;
        auto backend = makeBackend(readMusicFile(file), file);
        bool gba = backend->channels().size() == 6;
        require(gba || backend->channels().size() == 16, "PSF hardware channel count wrong");
        require(backend->tracks().size() == 1 && !backend->tracks()[0].title.empty(),
                "PSF tags missing");
        AudioBlock block;
        for (int i = 0; i < 94; i++)
            backend->render(512, block);
        size_t a = gba ? 0 : 8, b = gba ? 1 : 9;
        require(energy(block.mix) > 1e-5 && energy(block.voices[a]) > 1e-5 &&
                    energy(block.voices[b]) > 1e-5,
                "PSF original driver produced no real PCM");
        if (!gba)
            require(energy(block.voices[0]) > 1e-5, "DS PCM sample channel missing");
        double difference = 0;
        for (size_t i = 0; i < 512; i++)
            difference += std::abs(block.voices[a][i].l - block.voices[b][i].l);
        require(difference > .01, "PSF channel signals are copies");
        backend->muteMask((1u << backend->channels().size()) - 1);
        for (int i = 0; i < 30; i++)
            backend->render(512, block);
        require(energy(block.mix) < 1e-8 && energy(block.voices[a]) > 1e-5,
                "PSF mute destroyed source scopes or retained audio");
        backend->muteMask(0);
        backend->seek(500);
        backend->render(512, block);
        require(energy(block.mix) > 1e-5, "PSF seek/replay failed");
        backend->seek(4100);
        backend->render(512, block);
        require(energy(block.mix) == 0, "PSF length/fade failed");
        std::cout << name
                  << ": actual channel PCM, mute/source retention, metadata, seek/fade PASS\n";
    }
    for (auto name : {"missing.minigsf", "cycle.minigsf", "bad-crc.gsf", "huge-map.gsf",
                      "truncated-save.2sf"}) {
        bool rejected = false;
        try {
            auto file = root / name;
            makeBackend(readMusicFile(file), file);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Malformed PSF chain accepted");
    }
    Player player;
    for (auto name : {"channels.gsf", "channels.2sf"}) {
        player.open(root / name);
        player.play();
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        player.pause();
        require(player.seconds() > .2 && player.underruns() == 0,
                "New PSF player starved or failed to advance");
    }
}
static void folderPlaylistTests(const std::filesystem::path &root) {
    auto file = root / "playlist-ds/02 Track.mini2sf";
    auto playlist = FolderPlaylist::scan(file, Format::TwoSf);
    require(playlist.files.size() == 3 && playlist.current == 1,
            "Folder track selection or library exclusion failed");
    require(playlist.files[0].filename() == "01 Track.mini2sf" &&
                playlist.files[2].filename() == "10 Track.mini2sf",
            "Folder ordering failed");
    require(!FolderPlaylist::scan(root / "channels.gbs", Format::Gbs).active(),
            "GBS must use internal tracks");
    require(!FolderPlaylist::scan(root / "channels.nsf", Format::Nsf).active(),
            "NSF must use internal tracks");
    for (auto format : {Format::Spc, Format::Gsf, Format::Usf, Format::Bcstm, Format::Bcwav}) {
        auto file = root / (format == Format::Spc     ? "channels.spc"
                            : format == Format::Gsf   ? "channels.gsf"
                            : format == Format::Usf   ? "channels.usf"
                            : format == Format::Bcstm ? "channels.bcstm"
                                                      : "channels.bcwav");
        auto list = FolderPlaylist::scan(file, format);
        require(list.active() && list.current < list.files.size() &&
                    list.files[list.current] == std::filesystem::absolute(file),
                "Folder system unavailable");
        for (auto path : list.files)
            require(path.extension().wstring().find(L"lib") == std::wstring::npos,
                    "Music library entered playlist");
    }
    std::cout << "Folder playlists, numbered ordering, library exclusion and internal-track "
                 "exceptions: PASS\n";
}
static void dsOneShotTest(const std::filesystem::path &root) {
    auto file = root / "oneshot.2sf";
    auto backend = makeBackend(readMusicFile(file), file);
    AudioBlock block;
    backend->render(512, block);
    require(energy(block.voices[0]) > 1e-5, "One-shot PCM did not start");
    for (int i = 0; i < 60; i++)
        backend->render(512, block);
    require(energy(block.voices[0]) == 0 && energy(block.mix) > 1e-5,
            "One-shot end damaged other channels");
    std::cout << "DS one-shot channel completion and continued PSG playback: PASS\n";
}
static void outputFormatTests(const std::filesystem::path &root) {
    for (auto ext : {"bcstm", "bcwav"}) {
        auto original = root / (std::string("channels.") + ext);
        auto unicode = root / std::filesystem::u8path(std::string(u8"한글 음악.") + ext);
        std::filesystem::copy_file(original, unicode,
                                   std::filesystem::copy_options::overwrite_existing);
        auto backend = makeBackend(readMusicFile(unicode), unicode);
        AudioBlock block;
        backend->render(512, block);
        require(energy(block.mix) > 1e-5, "Unicode 3DS path failed to decode");
    }
    for (auto name : {"channels.usf", "channels.miniusf", "channels.bcstm", "channels.bcwav"}) {
        auto file = root / name;
        auto backend = makeBackend(readMusicFile(file), file);
        require(backend->channels().size() == 2, "Stereo output channel count wrong");
        AudioBlock block;
        for (int i = 0; i < 30; i++)
            backend->render(512, block);
        require(energy(block.mix) > 1e-5 && energy(block.voices[0]) > 1e-5 &&
                    energy(block.voices[1]) > 1e-5,
                "New output format produced silence");
        require(block.voices[0][100].r == 0 && block.voices[1][100].l == 0,
                "Left/right outputs are not independent");
        double delta = 0;
        for (size_t i = 0; i < 512; i++)
            delta += std::abs(block.voices[0][i].l - block.voices[1][i].r);
        require(delta > .01, "Independent fixture output channels are copies");
        backend->muteMask(1);
        backend->render(512, block);
        require(block.mix[100].l == 0 && energy(block.voices[0]) > 1e-5,
                "Output mute erased source wave");
        backend->muteMask(0);
        backend->seek(500);
        backend->render(512, block);
        require(energy(block.mix) > 1e-5, "Output format seek failed");
        backend->seek(5000);
        backend->render(512, block);
        require(energy(block.mix) == 0, "Output duration/end failed");
        std::cout << name
                  << ": real stereo PCM, independent outputs, metadata, mute/seek/end PASS\n";
    }
    for (auto name : {"bad-map.usf", "invalid.bcstm"}) {
        bool rejected = false;
        try {
            auto path = root / name;
            makeBackend(readMusicFile(path), path);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "Malformed output format accepted");
    }
    Player player;
    for (auto name : {"channels.usf", "channels.bcstm", "channels.bcwav"}) {
        player.open(root / name);
        player.play();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        player.pause();
        require(player.seconds() > .15 && player.underruns() == 0,
                "New output format player starved");
    }
}
static void bufferTests() {
    StereoRing<> ring;
    std::vector<Stereo> input(12000);
    for (size_t i = 0; i < input.size(); i++)
        input[i] = {float(std::sin(i * .057) * .4), float(std::cos(i * .037) * .3)};
    require(ring.push(input.data(), input.size()), "Ring fill failed");
    OutputResampler r;
    std::vector<float> out(4000);
    for (int rate : {16000, 22050, 32000, 44100, 48000, 96000}) {
        ring.reset();
        ring.push(input.data(), input.size());
        r.reset();
        r.rate(rate);
        r.pull(ring, out.data(), 2000, 1);
        require(r.underruns == 0, "Rate-dependent silence");
        double e = 0;
        for (float x : out)
            e += x * x;
        require(e > 10, "Resampling produced silence");
        require(std::abs(double(r.consumed) - 2000. * InternalRate / rate) <= 1,
                "Resampling clock drift");
    }
    ring.reset();
    ring.push(input.data(), input.size());
    r.reset();
    double expected = 0;
    for (int rate : {44100, 96000, 48000}) {
        r.rate(rate);
        r.pull(ring, out.data(), 1000, 1);
        expected += 1000. * InternalRate / rate;
    }
    require(r.underruns == 0 && std::abs(double(r.consumed) - expected) < 1,
            "Rate change reset timeline");
    StereoRing<128> concurrent;
    std::atomic<bool> okay{true};
    std::thread producer([&] {
        for (int i = 0; i < 100000; i++) {
            Stereo x{float(i), float(-i)};
            while (!concurrent.push(&x, 1))
                std::this_thread::yield();
        }
    });
    for (int i = 0; i < 100000; i++) {
        Stereo x;
        while (!concurrent.peek(0, x))
            std::this_thread::yield();
        if (x.l != i || x.r != -i)
            okay = false;
        concurrent.consume(1);
    }
    producer.join();
    require(okay, "SPSC data race/order failure");
    auto env = envelope(std::vector<float>(4096, 0), 800);
    for (auto x : env)
        require(x.minimum == 0 && x.maximum == 0, "Silent envelope unstable");
    std::vector<float> alternating(4096);
    for (size_t i = 0; i < alternating.size(); i++)
        alternating[i] = i % 2 ? 1.f : -1.f;
    env = envelope(alternating, 128);
    for (auto x : env)
        require(x.minimum == -1 && x.maximum == 1, "Envelope lost high-frequency peaks");
    std::cout << "44100/48000/96000 output, continuous rate changes, SPSC stress and min/max "
                 "envelopes: PASS\n";
}
static void levelTests(const std::filesystem::path &root) {
    OutputLevel level;
    level.configure(Format::Spc, false);
    auto value = level.process({.05f, -.025f});
    require(std::abs(value.l / .05f - std::pow(10.f, .5f)) < .0001f,
            "SPC trim did not increase quiet PCM by 10 dB");
    require(value.l == -2 * value.r, "Level trim changed stereo balance");
    value = level.process({0, 0});
    require(value.l == 0 && value.r == 0, "Level stage introduced signal into silence");
    for (int i = 0; i < 48000; ++i) {
        value = level.process({i % 2 ? 1.f : -1.f, .5f});
        require(std::abs(value.l) <= .980001f && std::abs(value.r) <= .980001f,
                "Limiter exceeded ceiling");
        require(std::abs(std::abs(value.l) / value.r - 2) < .0001f,
                "Limiter failed stereo linking");
    }
    for (int i = 0; i < 48000; ++i)
        value = level.process({.01f, .005f});
    require(value.l > .031f, "Limiter failed to release after a peak");
    for (auto file : {"channels.nsf", "fds.nsf", "channels.spc"}) {
        auto bytes = readMusicFile(root / file);
        auto backend = makeBackend(bytes);
        bool fds = std::string(file) == "fds.nsf";
        level.configure(identify(bytes), fds);
        AudioBlock block;
        backend->render(8192, block);
        auto before = energy(block.mix);
        for (auto &sample : block.mix) {
            sample = level.process(sample);
            require(std::max(std::abs(sample.l), std::abs(sample.r)) <= .980001f,
                    "Actual core audio exceeded output ceiling");
        }
        auto after = energy(block.mix);
        require(after >= before, "Format trim reduced fixture level");
        if (fds || identify(bytes) == Format::Spc)
            require(after > before * 1.5, "Actual format audio was not louder");
        wave(root / (std::string(file) + "-level.wav"), block.mix);
        std::cout << file << " output trim: " << 10 * std::log10(after / before) << " dB\n";
    }
    std::cout << "Format trim, silence/stereo preservation and stereo-linked peak limiter: PASS\n";
}
static void effectsTests() {
    Reverb effect;
    auto dry = effect.process({.4f, -.2f});
    require(dry.l == .4f && dry.r == -.2f, "Disabled reverb changed original audio");
    effect.enabled(true);
    effect.process({1, 1});
    double early = 0, late = 0;
    for (int i = 0; i < 96000; ++i) {
        auto value = effect.process({0, 0});
        if (i < 24000)
            early += value.l * value.l + value.r * value.r;
        if (i > 72000)
            late += value.l * value.l + value.r * value.r;
    }
    require(early > 1e-5 && late < early * .01, "Reverb tail missing or failed to decay");
    effect.enabled(false);
    for (int i = 0; i < 1000; ++i)
        effect.process({0, 0});
    dry = effect.process({.4f, -.2f});
    require(dry.l == .4f && dry.r == -.2f, "Reverb bypass retained an effect");
    auto tailEnergy = [](float amount) {
        Reverb adjustable;
        adjustable.enabled(true);
        adjustable.amount(amount);
        for (int i = 0; i < 2500; ++i)
            adjustable.process({0, 0});
        adjustable.process({1, 1});
        double energy = 0;
        for (int i = 0; i < 48000; ++i) {
            auto sample = adjustable.process({0, 0});
            require(std::isfinite(sample.l) && std::isfinite(sample.r), "Non-finite reverb output");
            energy += sample.l * sample.l + sample.r * sample.r;
        }
        return energy;
    };
    require(tailEnergy(0) == 0, "Zero reverb strength retained a tail");
    double quiet = tailEnergy(.25f), strong = tailEnergy(.75f);
    require(quiet > 0 && std::abs(strong / quiet - 9) < .01,
            "Numerical reverb strength did not scale reflected audio");
    require(tailEnergy(-1) == 0 && std::abs(tailEnergy(2) - tailEnergy(1)) < 1e-8,
            "Reverb strength bounds failed");
    auto lingeringEnergy = [](float seconds) {
        Reverb room;
        room.enabled(true);
        room.decay(seconds);
        for (int i = 0; i < 2500; ++i)
            room.process({0, 0});
        room.process({1, 1});
        double energy = 0;
        for (int i = 0; i < 48000; ++i) {
            auto sample = room.process({0, 0});
            if (i > 24000)
                energy += sample.l * sample.l + sample.r * sample.r;
        }
        return energy;
    };
    require(lingeringEnergy(1.2f) > 1e-8 && lingeringEnergy(1.2f) > lingeringEnergy(.2f) * 100,
            "Long decay did not leave audible reflections after source ended");
    StereoRing<> ring;
    std::vector<Stereo> signal(12000);
    for (size_t i = 0; i < signal.size(); ++i)
        signal[i] = {float(i) / 12000, float(i) / 12000};
    ring.push(signal.data(), signal.size());
    OutputResampler resampler;
    resampler.rate(96000);
    std::vector<float> out(2000);
    resampler.pull(ring, out.data(), 1000, 1, 1.1);
    require(std::abs(double(resampler.consumed) - 550) < 1, "1.1x pitch/speed clock wrong");
    resampler.pull(ring, out.data(), 1000, 1, .9);
    require(std::abs(double(resampler.consumed) - 1000) < 1, "Live speed change reset clock");
    require(resampler.underruns == 0, "Speed change starved resampler");
    ring.reset();
    resampler.reset();
    for (size_t i = 0; i < signal.size(); ++i)
        signal[i] = {float(std::sin(i * 2 * 3.141592653589793 * 1000 / InternalRate)), 0};
    ring.push(signal.data(), signal.size());
    out.resize(12000);
    resampler.pull(ring, out.data(), 6000, 1, 1.1);
    int rising = 0;
    for (int i = 1; i < 6000; ++i)
        if (out[2 * i - 2] <= 0 && out[2 * i] > 0)
            ++rising;
    require(std::abs(rising - 69) <= 1, "Pitch did not rise with speed");
    std::cout << "Reverb bypass/tail/decay and live 0.1-step pitch/speed clock: PASS\n";
}
static void playerTests(const std::filesystem::path &root) {
    Player p;
    require(p.master() == 1.f, "Default master must be 100%");
    require(p.rate() == 96000, "Default output must be 96kHz");
    p.speed(1.1f);
    require(p.speed() == 1.1f, "Player speed control failed");
    for (int rate : {16000, 22050, 32000, 44100, 48000, 96000}) {
        p.rate(rate);
        require(p.rate() == rate, "Lower output rate unavailable");
    }
    p.speed(1);
    p.reverb(true);
    require(p.reverb(), "Reverb control failed");
    p.reverb(false);
    std::cout << "Audio driver: " << SDL_GetCurrentAudioDriver() << "\n";
    p.open(root / "channels.nsfe");
    require(p.outputGain() == 1, "Plain NES trim changed");
    require(!p.playing(), "Open must not force play");
    require(!p.dmcUsed(), "Unused DMC must be hidden");
    p.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    p.pause();
    double time = p.seconds();
    require(time > .1, "Device callback did not consume audio");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(p.seconds() == time, "Pause advanced time");
    auto wave = p.waveforms();
    require(wave.size() == 5, "Missing player histories");
    p.rate(44100);
    require(p.seconds() == time, "Rate change reset core position");
    p.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    p.rate(96000);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    p.pause();
    require(p.seconds() > time && p.underruns() == 0,
            "Rate switch caused output silence/underruns");
    auto state = p.states()[0];
    state.volume = 2;
    p.channel(0, state);
    require(p.states()[0].volume == 2, "200% voice gain rejected");
    state.volume = .4f;
    state.mute = true;
    p.channel(0, state);
    require(p.states()[0].volume == .4f && p.states()[0].mute, "Independent controls lost");
    p.select(1);
    require(p.track() == 1 && p.seconds() == 0, "Track selection failed");
    p.seek(300);
    require(p.seconds() >= .3, "Seek position failed");
    p.stop();
    require(!p.playing() && p.seconds() == 0, "Stop did not reset");
    p.select(0);
    p.play();
    for (int i = 0; i < 200 && !p.ended(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(p.ended(), "Metadata-defined end not reached");
    p.pause();
    p.play();
    require(p.playing() && p.seconds() < .1, "Replay after end failed");
    p.pause();
    require(p.error().empty(), "Worker reported failure");
    p.open(root / "fds.nsf");
    p.rate(16000);
    p.speed(2);
    p.reverb(true);
    p.play();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    p.pause();
    require(p.underruns() == 0 && p.seconds() > .2, "Low-rate 2x playback underrun");
    p.speed(1);
    p.reverb(false);
    p.rate(96000);
    require(p.outputGain() > 1.58f && p.outputGain() < 1.59f, "FDS gain not applied by player");
    p.open(root / "dmc.nsf");
    require(p.dmcUsed(), "Actual DMC sample was not detected");
    auto dmcState = p.states()[4];
    dmcState.mute = true;
    p.channel(4, dmcState);
    p.seek(500);
    require(p.dmcUsed(), "DMC activity latch lost on same-track seek");
    p.open(root / "channels.spc");
    require(!p.dmcUsed(), "New file retained previous DMC activity");
    require(p.outputGain() > 3.16f && p.outputGain() < 3.17f, "SPC gain not applied by player");
    std::cout << "Actual SDL callback, pause/resume, rate switch, channel controls, seek, stop, "
                 "metadata end and replay: PASS\n";
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Fixture directory required");
        std::filesystem::path root = argv[1];
        formatTests(root);
        newAudioTests(root);
        psfTests(root);
        folderPlaylistTests(root);
        dsOneShotTest(root);
        outputFormatTests(root);
        bufferTests();
        levelTests(root);
        effectsTests();
        playerTests(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
