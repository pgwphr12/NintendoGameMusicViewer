#include "ui/Window.hpp"
#include "WindowsIntegration.hpp"
#include <shellapi.h>
#include <shobjidl.h>
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    try {
        int count = 0;
        LPWSTR *raw = CommandLineToArgvW(GetCommandLineW(), &count);
        std::filesystem::path input, screenshot;
        int width = 1280, height = 720;
        bool recording = false, playInitial = false;
        if (count > 1) {
            if (std::wstring(raw[1]) == L"--snapshot" && count > 3) {
                input = raw[2];
                screenshot = raw[3];
                for (int i = 4; i < count; i++) {
                    std::wstring flag = raw[i];
                    if (flag == L"--recording")
                        recording = true;
                    if (flag.rfind(L"--size=", 0) == 0) {
                        auto x = flag.find(L'x', 7);
                        if (x == std::wstring::npos)
                            throw std::runtime_error("Use --size=WIDTHxHEIGHT");
                        width = std::stoi(flag.substr(7, x - 7));
                        height = std::stoi(flag.substr(x + 1));
                        if (width < 800 || height < 450 || width > 3840 || height > 2160)
                            throw std::runtime_error("Snapshot size outside allowed range");
                    }
                }
            } else if (std::wstring(raw[1]) == L"--play" && count > 2) {
                input = raw[2];
                playInitial = true;
            } else
                input = raw[1];
        }
        LocalFree(raw);
        ngmv::desktop::InstanceGate gate;
        if (screenshot.empty() && gate.forwardOrBecomePrimary(input)) return 0;
        SetCurrentProcessExplicitAppUserModelID(L"NintendoGameMusicViewer");
        ngmv::Window app;
        return app.run(instance, input, screenshot, width, height, recording, playInitial);
    } catch (const std::exception &e) {
        MessageBoxW(nullptr, ngmv::wide(e.what()).c_str(), L"Nintendo Game Music Viewer",
                    MB_ICONERROR);
        return 1;
    }
}
