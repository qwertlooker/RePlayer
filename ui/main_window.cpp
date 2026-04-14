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
constexpr wchar_t kMainWindowTitle[] = L"RePlayer v1.5";
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
    IdPlayRecording,
    IdPrevSentence,
    IdNextSentence
};

HWND CreateButton(HWND parent, HINSTANCE instance, const wchar_t* text, int id) {
    return CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           0, 0, 100, 28, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}

HWND CreateLabel(HWND parent, HINSTANCE instance, const wchar_t* text, int width = 200) {
    return CreateWindowExW(0, WC_STATICW, text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                           0, 0, width, 24, parent, nullptr, instance, nullptr);
}

HWND CreateEdit(HWND parent, HINSTANCE instance, const wchar_t* text, int id, int width = 80) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           0, 0, width, 24, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
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
        1200,
        800,
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
        case IdPrevSentence:
            if (const auto result = coordinator_->SelectPreviousSubtitle(); !result.Ok()) {
                ShowError(result.Error());
            }
            SyncUi();
            return 0;
        case IdNextSentence:
            if (const auto result = coordinator_->SelectNextSubtitle(); !result.Ok()) {
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
        if (font_large_) DeleteObject(font_large_);
        if (font_normal_) DeleteObject(font_normal_);
        if (font_small_) DeleteObject(font_small_);
        KillTimer(hwnd_, kUiTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void MainWindow::CreateControls() {
    font_large_ = CreateFontW(28, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_normal_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_small_ = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    button_open_audio_ = CreateButton(hwnd_, instance_, L"Open MP3", IdOpenAudio);
    button_open_subtitle_ = CreateButton(hwnd_, instance_, L"Open Subtitle", IdOpenSubtitle);

    label_audio_name_ = CreateLabel(hwnd_, instance_, L"No audio loaded", 300);
    label_subtitle_name_ = CreateLabel(hwnd_, instance_, L"No subtitle loaded", 300);
    label_time_ = CreateLabel(hwnd_, instance_, L"00:00 / 00:00", 160);

    label_current_sentence_ = CreateWindowExW(
        0, WC_STATICW, L"Select a subtitle to begin learning", WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 600, 80, hwnd_, nullptr, instance_, nullptr);
    SendMessageW(label_current_sentence_, WM_SETFONT, reinterpret_cast<WPARAM>(font_large_), TRUE);

    label_sentence_time_ = CreateLabel(hwnd_, instance_, L"", 200);
    label_sentence_index_ = CreateLabel(hwnd_, instance_, L"", 100);
    label_mode_ = CreateLabel(hwnd_, instance_, L"Mode: Normal", 150);
    label_recording_status_ = CreateLabel(hwnd_, instance_, L"No recording", 150);
    label_hint_ = CreateLabel(hwnd_, instance_, L"", 400);

    button_play_ = CreateButton(hwnd_, instance_, L"Play", IdPlay);
    button_pause_ = CreateButton(hwnd_, instance_, L"Pause", IdPause);
    button_stop_ = CreateButton(hwnd_, instance_, L"Stop", IdStop);
    button_prev_sentence_ = CreateButton(hwnd_, instance_, L"< Prev", IdPrevSentence);
    button_next_sentence_ = CreateButton(hwnd_, instance_, L"Next >", IdNextSentence);
    button_play_original_ = CreateButton(hwnd_, instance_, L"Play Original", IdPlayOriginal);
    button_record_start_ = CreateButton(hwnd_, instance_, L"Start Recording", IdRecordStart);
    button_record_stop_ = CreateButton(hwnd_, instance_, L"Stop Recording", IdRecordStop);
    button_play_recording_ = CreateButton(hwnd_, instance_, L"Play Recording", IdPlayRecording);

    track_position_ = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0, 100, 24, hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTrackPosition)), instance_, nullptr);
    SendMessageW(track_position_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));

    combo_mode_ = CreateWindowExW(
        0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        0, 0, 150, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdModeCombo)), instance_, nullptr);
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Normal"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"RepeatOne"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"AutoPause"));
    SendMessageW(combo_mode_, CB_SETCURSEL, 0, 0);

    edit_auto_pause_ = CreateEdit(hwnd_, instance_, L"1", IdAutoPauseEdit, 60);

    list_subtitles_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTBOXW, L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        0, 0, 400, 300, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdSubtitleList)), instance_, nullptr);

    SendMessageW(label_audio_name_, WM_SETFONT, reinterpret_cast<WPARAM>(font_small_), TRUE);
    SendMessageW(label_subtitle_name_, WM_SETFONT, reinterpret_cast<WPARAM>(font_small_), TRUE);
    SendMessageW(label_time_, WM_SETFONT, reinterpret_cast<WPARAM>(font_normal_), TRUE);
    SendMessageW(label_sentence_time_, WM_SETFONT, reinterpret_cast<WPARAM>(font_small_), TRUE);
    SendMessageW(label_sentence_index_, WM_SETFONT, reinterpret_cast<WPARAM>(font_small_), TRUE);
    SendMessageW(label_mode_, WM_SETFONT, reinterpret_cast<WPARAM>(font_normal_), TRUE);
    SendMessageW(label_recording_status_, WM_SETFONT, reinterpret_cast<WPARAM>(font_normal_), TRUE);
    SendMessageW(label_hint_, WM_SETFONT, reinterpret_cast<WPARAM>(font_small_), TRUE);
}

void MainWindow::LayoutControls(int width, int height) {
    const int padding = 12;
    const int button_width = 110;
    const int button_height = 28;
    const int row1_y = padding;
    const int row2_y = padding + 36;
    const int current_sentence_area_y = row2_y + 40;
    const int current_sentence_area_height = 120;
    const int controls_y = current_sentence_area_y + current_sentence_area_height + padding;
    const int list_y = controls_y + 100;

    MoveWindow(button_open_audio_, padding, row1_y, button_width, button_height, TRUE);
    MoveWindow(button_open_subtitle_, padding + button_width + 8, row1_y, 130, button_height, TRUE);
    MoveWindow(label_audio_name_, padding + button_width + 150, row1_y + 4, 250, 20, TRUE);
    MoveWindow(label_subtitle_name_, padding + button_width + 410, row1_y + 4, 250, 20, TRUE);

    MoveWindow(track_position_, padding, row2_y, width - padding * 2, 32, TRUE);
    MoveWindow(label_time_, padding, row2_y + 36, 160, 20, TRUE);

    MoveWindow(label_current_sentence_, padding, current_sentence_area_y, width - padding * 2, current_sentence_area_height, TRUE);
    MoveWindow(label_sentence_index_, padding, current_sentence_area_y + current_sentence_area_height - 50, 100, 20, TRUE);
    MoveWindow(label_sentence_time_, padding + 110, current_sentence_area_y + current_sentence_area_height - 50, 200, 20, TRUE);
    MoveWindow(label_mode_, padding + 320, current_sentence_area_y + current_sentence_area_height - 50, 150, 20, TRUE);
    MoveWindow(label_recording_status_, padding + 480, current_sentence_area_y + current_sentence_area_height - 50, 150, 20, TRUE);
    MoveWindow(label_hint_, padding + 640, current_sentence_area_y + current_sentence_area_height - 50, width - 660, 20, TRUE);

    int btn_x = padding;
    const int btn_y = controls_y;
    MoveWindow(button_play_, btn_x, btn_y, 80, button_height, TRUE);
    btn_x += 88;
    MoveWindow(button_pause_, btn_x, btn_y, 80, button_height, TRUE);
    btn_x += 88;
    MoveWindow(button_stop_, btn_x, btn_y, 80, button_height, TRUE);
    btn_x += 88;
    MoveWindow(button_prev_sentence_, btn_x, btn_y, 80, button_height, TRUE);
    btn_x += 88;
    MoveWindow(button_next_sentence_, btn_x, btn_y, 80, button_height, TRUE);
    btn_x += 88;
    MoveWindow(button_play_original_, btn_x, btn_y, 110, button_height, TRUE);
    btn_x += 118;
    MoveWindow(button_record_start_, btn_x, btn_y, 120, button_height, TRUE);
    btn_x += 128;
    MoveWindow(button_record_stop_, btn_x, btn_y, 120, button_height, TRUE);
    btn_x += 128;
    MoveWindow(button_play_recording_, btn_x, btn_y, 120, button_height, TRUE);

    MoveWindow(combo_mode_, padding, controls_y + 36, 150, 200, TRUE);
    MoveWindow(edit_auto_pause_, padding + 160, controls_y + 36, 60, 24, TRUE);

    MoveWindow(list_subtitles_, padding, list_y, width - padding * 2, height - list_y - padding, TRUE);
}

void MainWindow::OnTimer() {
    coordinator_->Tick();
    SyncUi();
}

void MainWindow::SyncUi() {
    const auto& state = coordinator_->GetState();

    const auto time_text = FormatTimestamp(state.position_ms) + L" / " + FormatTimestamp(state.duration_ms);
    SetWindowTextW(label_time_, time_text.c_str());

    SetWindowTextW(label_audio_name_, state.audio_display_name.c_str());
    SetWindowTextW(label_subtitle_name_, state.subtitle_display_name.c_str());

    if (!state.current_sentence_text.empty()) {
        SetWindowTextW(label_current_sentence_, state.current_sentence_text.c_str());
    } else {
        SetWindowTextW(label_current_sentence_, L"Select a subtitle to begin learning");
    }

    if (state.current_subtitle_index.has_value()) {
        const auto index_text = L"Sentence #" + std::to_wstring(*state.current_subtitle_index);
        SetWindowTextW(label_sentence_index_, index_text.c_str());

        const auto time_range = FormatTimestamp(state.current_sentence_start_ms) + L" - " + FormatTimestamp(state.current_sentence_end_ms);
        SetWindowTextW(label_sentence_time_, time_range.c_str());
    } else {
        SetWindowTextW(label_sentence_index_, L"");
        SetWindowTextW(label_sentence_time_, L"");
    }

    const auto mode_text = L"Mode: " + state.mode_status_text;
    SetWindowTextW(label_mode_, mode_text.c_str());
    SetWindowTextW(label_recording_status_, state.recording_status_text.c_str());
    SetWindowTextW(label_hint_, state.hint_text.c_str());

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
    wchar_t current_text[16]{};
    GetWindowTextW(edit_auto_pause_, current_text, static_cast<int>(std::size(current_text)));
    if (pause_seconds != current_text) {
        SetWindowTextW(edit_auto_pause_, pause_seconds.c_str());
    }

    EnableWindow(button_play_, state.can_play);
    EnableWindow(button_pause_, state.can_pause);
    EnableWindow(button_stop_, state.can_stop);
    EnableWindow(button_prev_sentence_, state.can_go_previous_sentence);
    EnableWindow(button_next_sentence_, state.can_go_next_sentence);
    EnableWindow(button_play_original_, state.can_play_original_segment);
    EnableWindow(button_record_start_, state.can_start_recording);
    EnableWindow(button_record_stop_, state.can_stop_recording);
    EnableWindow(button_play_recording_, state.can_play_recording);

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
        const auto display_text = FormatTimePrefix(line.start_ms) + L"  " + line.text;
        SendMessageW(list_subtitles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(display_text.c_str()));
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

std::wstring MainWindow::FormatTimePrefix(std::int64_t ms) const {
    const auto total_seconds = static_cast<int>(ms / 1000);
    const auto minutes = total_seconds / 60;
    const auto seconds = total_seconds % 60;
    const auto milliseconds = static_cast<int>(ms % 1000);

    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02d:%02d.%03d", minutes, seconds, milliseconds);
    return buffer;
}

} // namespace replayer
