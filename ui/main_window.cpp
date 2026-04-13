#include "ui/main_window.h"

#include <commctrl.h>
#include <commdlg.h>

#include <memory>

#include "audio/mf_player.h"
#include "core/clock.h"
#include "core/time_format.h"
#include "recording/wasapi_recorder.h"
#include "utils/logger.h"

namespace replayer {

namespace {

constexpr wchar_t kMainWindowClassName[] = L"RePlayerMainWindow";
constexpr wchar_t kMainWindowTitle[] = L"RePlayer v1";
constexpr UINT_PTR kUiTimerId = 1001;

enum ControlId : int {
    IdOpenAudio = 2001,
    IdOpenSubtitle,
    IdPlay,
    IdPause,
    IdStop,
    IdTrackPosition,
    IdSubtitleList,
    IdModeCombo,
    IdAutoPauseEdit,
    IdRecordStart,
    IdRecordStop,
    IdPlayOriginal,
    IdPlayRecording
};

HWND CreateButton(HWND parent, HINSTANCE instance, const wchar_t* text, int id) {
    return CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           0, 0, 100, 28, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}

} // namespace

MainWindow::MainWindow(HINSTANCE instance) : instance_(instance) {}

Result<void> MainWindow::Create(int cmd_show) {
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    auto logger = std::make_shared<Logger>();
    coordinator_ = std::make_unique<PlaybackCoordinator>(
        std::make_unique<MfPlayer>(),
        std::make_unique<WasapiRecorder>(),
        std::make_unique<SystemClock>(),
        logger);

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &MainWindow::WindowProc;
    window_class.hInstance = instance_;
    window_class.lpszClassName = kMainWindowClassName;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (RegisterClassExW(&window_class) == 0) {
        return MakeError(ErrorCode::SystemError, L"Failed to register main window class.");
    }

    hwnd_ = CreateWindowExW(
        0,
        kMainWindowClassName,
        kMainWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        720,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr) {
        return MakeError(ErrorCode::SystemError, L"Failed to create main window.");
    }

    ShowWindow(hwnd_, cmd_show);
    UpdateWindow(hwnd_);
    SetTimer(hwnd_, kUiTimerId, 100, nullptr);
    return Ok();
}

HWND MainWindow::Handle() const noexcept {
    return hwnd_;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    MainWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<MainWindow*>(create_struct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        CreateControls();
        return 0;
    case WM_SIZE:
        LayoutControls(LOWORD(l_param), HIWORD(l_param));
        return 0;
    case WM_TIMER:
        if (w_param == kUiTimerId) {
            OnTimer();
        }
        return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(l_param) == track_position_) {
            const auto code = LOWORD(w_param);
            if (code == TB_THUMBTRACK) {
                is_dragging_track_ = true;
            } else if (code == TB_ENDTRACK || code == TB_THUMBPOSITION) {
                is_dragging_track_ = false;
                const auto value = static_cast<std::int64_t>(SendMessageW(track_position_, TBM_GETPOS, 0, 0));
                const auto duration = coordinator_->GetState().duration_ms;
                const auto position = duration > 0 ? (duration * value) / 1000 : 0;
                if (const auto result = coordinator_->Seek(position); !result.Ok()) {
                    ShowError(result.Error());
                }
                SyncUi();
            }
        }
        return 0;
    case WM_COMMAND: {
        const auto id = LOWORD(w_param);
        const auto code = HIWORD(w_param);
        switch (id) {
        case IdOpenAudio: {
            const auto path = OpenFileDialog(L"Audio Files\0*.mp3;*.wav\0All Files\0*.*\0");
            if (!path.empty()) {
                if (const auto result = coordinator_->LoadAudio(path); !result.Ok()) {
                    ShowError(result.Error());
                }
                SyncUi();
            }
            return 0;
        }
        case IdOpenSubtitle: {
            const auto path = OpenFileDialog(L"Subtitle Files\0*.srt;*.lrc\0All Files\0*.*\0");
            if (!path.empty()) {
                if (const auto result = coordinator_->LoadSubtitles(path); !result.Ok()) {
                    ShowError(result.Error());
                }
                PopulateSubtitleList();
                SyncUi();
            }
            return 0;
        }
        case IdPlay:
            if (const auto result = coordinator_->Play(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdPause:
            if (const auto result = coordinator_->Pause(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdStop:
            if (const auto result = coordinator_->Stop(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdSubtitleList:
            if (code == LBN_SELCHANGE) {
                const auto selection = static_cast<int>(SendMessageW(list_subtitles_, LB_GETCURSEL, 0, 0));
                if (selection != LB_ERR) {
                    const auto& subtitles = coordinator_->GetState().subtitles;
                    if (selection >= 0 && selection < static_cast<int>(subtitles.size())) {
                        if (const auto result = coordinator_->SelectSubtitle(subtitles[selection].index); !result.Ok()) {
                            ShowError(result.Error());
                        }
                    }
                }
                SyncUi();
            }
            return 0;
        case IdModeCombo:
            if (code == CBN_SELCHANGE) {
                coordinator_->SetPlaybackMode(SelectedPlaybackMode());
                SyncUi();
            }
            return 0;
        case IdAutoPauseEdit:
            if (code == EN_CHANGE) {
                wchar_t buffer[16]{};
                GetWindowTextW(edit_auto_pause_, buffer, static_cast<int>(std::size(buffer)));
                coordinator_->SetAutoPauseDelayMs(_wtoi64(buffer) * 1000LL);
            }
            return 0;
        case IdRecordStart:
            if (const auto result = coordinator_->StartRecording(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdRecordStop:
            if (const auto result = coordinator_->StopRecording(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdPlayOriginal:
            if (const auto result = coordinator_->PlayOriginalSegment(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdPlayRecording:
            if (const auto result = coordinator_->PlayRecording(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        default:
            break;
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd_, kUiTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void MainWindow::CreateControls() {
    button_open_audio_ = CreateButton(hwnd_, instance_, L"Open MP3", IdOpenAudio);
    button_open_subtitle_ = CreateButton(hwnd_, instance_, L"Open Subtitle", IdOpenSubtitle);
    button_play_ = CreateButton(hwnd_, instance_, L"Play", IdPlay);
    button_pause_ = CreateButton(hwnd_, instance_, L"Pause", IdPause);
    button_stop_ = CreateButton(hwnd_, instance_, L"Stop", IdStop);
    button_record_start_ = CreateButton(hwnd_, instance_, L"Start Recording", IdRecordStart);
    button_record_stop_ = CreateButton(hwnd_, instance_, L"Stop Recording", IdRecordStop);
    button_play_original_ = CreateButton(hwnd_, instance_, L"Play Original", IdPlayOriginal);
    button_play_recording_ = CreateButton(hwnd_, instance_, L"Play Recording", IdPlayRecording);

    track_position_ = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 100, 24, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTrackPosition)), instance_, nullptr);
    SendMessageW(track_position_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));

    label_time_ = CreateWindowExW(0, WC_STATICW, L"00:00 / 00:00", WS_CHILD | WS_VISIBLE,
                                  0, 0, 120, 24, hwnd_, nullptr, instance_, nullptr);
    label_status_ = CreateWindowExW(0, WC_STATICW, L"Ready", WS_CHILD | WS_VISIBLE,
                                    0, 0, 300, 24, hwnd_, nullptr, instance_, nullptr);

    list_subtitles_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTBOXW, L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        0, 0, 400, 300, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdSubtitleList)), instance_, nullptr);

    combo_mode_ = CreateWindowExW(
        0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        0, 0, 180, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdModeCombo)), instance_, nullptr);
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Normal"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"RepeatOne"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"AutoPause"));
    SendMessageW(combo_mode_, CB_SETCURSEL, 0, 0);

    edit_auto_pause_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_EDITW, L"1", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 80, 24, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdAutoPauseEdit)), instance_, nullptr);
}

void MainWindow::LayoutControls(int width, int height) {
    const int padding = 12;
    const int button_width = 110;
    const int button_height = 28;

    MoveWindow(button_open_audio_, padding, padding, button_width, button_height, TRUE);
    MoveWindow(button_open_subtitle_, padding + button_width + 8, padding, 130, button_height, TRUE);
    MoveWindow(button_play_, padding, 52, 90, button_height, TRUE);
    MoveWindow(button_pause_, padding + 98, 52, 90, button_height, TRUE);
    MoveWindow(button_stop_, padding + 196, 52, 90, button_height, TRUE);
    MoveWindow(track_position_, padding, 96, width - padding * 2, 32, TRUE);
    MoveWindow(label_time_, padding, 132, 160, 24, TRUE);
    MoveWindow(combo_mode_, padding + 180, 132, 150, 240, TRUE);
    MoveWindow(edit_auto_pause_, padding + 340, 132, 80, 24, TRUE);
    MoveWindow(label_status_, padding + 440, 132, width - 460, 24, TRUE);
    MoveWindow(button_record_start_, padding, 170, 130, button_height, TRUE);
    MoveWindow(button_record_stop_, padding + 138, 170, 130, button_height, TRUE);
    MoveWindow(button_play_original_, padding + 276, 170, 130, button_height, TRUE);
    MoveWindow(button_play_recording_, padding + 414, 170, 130, button_height, TRUE);
    MoveWindow(list_subtitles_, padding, 214, width - padding * 2, height - 226, TRUE);
}

void MainWindow::OnTimer() {
    coordinator_->Tick();
    SyncUi();
}

void MainWindow::SyncUi() {
    const auto& state = coordinator_->GetState();
    const auto time_text = FormatTimestamp(state.position_ms) + L" / " + FormatTimestamp(state.duration_ms);
    SetWindowTextW(label_time_, time_text.c_str());

    std::wstring status = state.status_text;
    if (state.is_recording) {
        status += L" | Recording: ON";
    }
    SetWindowTextW(label_status_, status.empty() ? L"Ready" : status.c_str());

    if (!is_dragging_track_) {
        const auto track_value = state.duration_ms > 0 ? static_cast<LPARAM>((state.position_ms * 1000) / state.duration_ms) : 0;
        SendMessageW(track_position_, TBM_SETPOS, TRUE, track_value);
    }

    int selected_mode = 0;
    switch (state.playback_mode) {
    case PlaybackMode::RepeatOne:
        selected_mode = 1;
        break;
    case PlaybackMode::AutoPause:
        selected_mode = 2;
        break;
    case PlaybackMode::Normal:
    default:
        selected_mode = 0;
        break;
    }
    SendMessageW(combo_mode_, CB_SETCURSEL, selected_mode, 0);

    const auto pause_seconds = std::to_wstring(state.auto_pause_delay_ms / 1000);
    SetWindowTextW(edit_auto_pause_, pause_seconds.c_str());

    if (state.current_subtitle_index.has_value()) {
        const auto& lines = state.subtitles;
        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            if (lines[i].index == *state.current_subtitle_index) {
                SendMessageW(list_subtitles_, LB_SETCURSEL, i, 0);
                break;
            }
        }
    }
}

void MainWindow::PopulateSubtitleList() {
    SendMessageW(list_subtitles_, LB_RESETCONTENT, 0, 0);
    const auto& subtitles = coordinator_->GetState().subtitles;
    for (const auto& line : subtitles) {
        SendMessageW(list_subtitles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.text.c_str()));
    }
}

void MainWindow::ShowError(const AppError& error) {
    MessageBoxW(hwnd_, error.message.c_str(), kMainWindowTitle, MB_OK | MB_ICONERROR);
}

std::wstring MainWindow::OpenFileDialog(const wchar_t* filter) const {
    wchar_t file_path[MAX_PATH]{};
    OPENFILENAMEW open_file{};
    open_file.lStructSize = sizeof(open_file);
    open_file.hwndOwner = hwnd_;
    open_file.lpstrFilter = filter;
    open_file.lpstrFile = file_path;
    open_file.nMaxFile = MAX_PATH;
    open_file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&open_file)) {
        return file_path;
    }
    return L"";
}

PlaybackMode MainWindow::SelectedPlaybackMode() const {
    const auto selection = static_cast<int>(SendMessageW(combo_mode_, CB_GETCURSEL, 0, 0));
    switch (selection) {
    case 1:
        return PlaybackMode::RepeatOne;
    case 2:
        return PlaybackMode::AutoPause;
    case 0:
    default:
        return PlaybackMode::Normal;
    }
}

} // namespace replayer
