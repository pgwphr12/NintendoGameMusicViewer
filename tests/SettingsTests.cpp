#include "app/Settings.hpp"
#include <stdexcept>
static void check(bool value) { if (!value) throw std::runtime_error("Settings check failed"); }
int main() {
    using ngmv::desktop::Settings;
    auto folder = std::filesystem::temp_directory_path() / (L"ngmv-settings-" + std::to_wstring(GetCurrentProcessId()));
    auto file = folder / L"설정.ini";
    Settings data; data.load(file); check(data.get("master", 100, 0, 100) == 100);
    data.values["master"] = 37; data.values["speed"] = 123;
    auto nes = Settings::channelKey("Nintendo NES", "TRIANGLE");
    auto snes = Settings::channelKey("Super Nintendo", "TRIANGLE");
    check(nes != snes);
    data.values[nes + ".volume"] = 175;
    check(data.save(file)); Settings copy; copy.load(file);
    check(copy.values == data.values);
    data.values["master"] = 45; check(data.save(file)); copy.load(file);
    check(copy.get("master", 100, 0, 100) == 45);
    { std::ofstream f(file); f << "master=999\nspeed=NaN\nrate=48000junk\nreverb=-9\nautoplay=0\n"; }
    copy.load(file); check(copy.get("master",100,0,100)==100);
    check(copy.get("speed",100,50,200)==100);
    check(copy.get("rate",96000,16000,96000)==96000);
    check(copy.get("autoplay",1,0,1)==0);
    { std::ofstream f(file); f << std::string(70000, 'x'); }
    copy.load(file); check(copy.values.empty());
    check(!copy.save(folder)); // A directory cannot replace a settings file.
    check(!std::filesystem::exists(folder.wstring()+L".tmp."+std::to_wstring(GetCurrentProcessId())));
    std::filesystem::remove(file); std::filesystem::remove(folder);
}
