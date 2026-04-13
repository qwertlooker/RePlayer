#include <windows.h>

#include "ui/main_window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmd_show) {
    replayer::MainWindow main_window(instance);
    if (const auto result = main_window.Create(cmd_show); !result.Ok()) {
        MessageBoxW(nullptr, result.Error().message.c_str(), L"RePlayer v1", MB_ICONERROR | MB_OK);
        return -1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
