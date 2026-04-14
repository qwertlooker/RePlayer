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
constexpr wchar_t kMainWindowTitle[] = L"RePlayer";
constexpr UINT_PTR kUiTimerId = 1001;

enum ControlId : int {
    IdOpenAudio = 2001, IdOpenSubtitle, IdPlay, IdTrackPosition,
    IdSubtitleList, IdModeCombo, IdAutoPauseEdit, IdRecordStart, IdRecordStop,
    IdPlayOriginal, IdPlayRecording, IdPrevSentence, IdNextSentence
};

HWND Btn(HWND p, HINSTANCE i, const wchar_t* t, int id, int w = 90, int h = 32) {
    return CreateWindowExW(0, WC_BUTTONW, t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           0, 0, w, h, p, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), i, nullptr);
}

HWND Lbl(HWND p, HINSTANCE i, const wchar_t* t, int w = 200) {
    return CreateWindowExW(0, WC_STATICW, t, WS_CHILD | WS_VISIBLE, 0, 0, w, 22, p, nullptr, i, nullptr);
}

HWND Edt(HWND p, HINSTANCE i, const wchar_t* t, int id, int w = 60) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, t, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER | ES_NUMBER,
                           0, 0, w, 28, p, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), i, nullptr);
}

} // namespace

MainWindow::MainWindow(HINSTANCE instance) : instance_(instance) {}
MainWindow::~MainWindow() {
    if (font_large_) DeleteObject(font_large_);
    if (font_normal_) DeleteObject(font_normal_);
    if (font_small_) DeleteObject(font_small_);
    if (font_mono_) DeleteObject(font_mono_);
    if (font_caption_) DeleteObject(font_caption_);
}

Result<void> MainWindow::Create(int cmd_show) {
    INITCOMMONCONTROLSEX icc{}; icc.dwSize = sizeof(icc); icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    auto logger = std::make_shared<Logger>();
    coordinator_ = std::make_unique<PlaybackCoordinator>(
        std::make_unique<MfPlayer>(), std::make_unique<WasapiRecorder>(),
        std::make_unique<SystemClock>(), logger);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = WindowProc; wc.hInstance = instance_;
    wc.lpszClassName = kMainWindowClassName; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&wc)) return MakeError(ErrorCode::SystemError, L"RegisterClass failed");

    hwnd_ = CreateWindowExW(WS_EX_COMPOSITED, kMainWindowClassName, kMainWindowTitle,
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1120, 760,
                            nullptr, nullptr, instance_, this);
    if (!hwnd_) return MakeError(ErrorCode::SystemError, L"CreateWindow failed");

    ShowWindow(hwnd_, cmd_show); UpdateWindow(hwnd_);
    SetTimer(hwnd_, kUiTimerId, 100, nullptr);
    return Ok();
}
HWND MainWindow::Handle() const noexcept { return hwnd_; }

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandleMessage(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: CreateControls(); return 0;
    case WM_SIZE:
        LayoutControls(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_TIMER: if (wp == kUiTimerId) OnTimer(); return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lp) == track_position_) {
            if (LOWORD(wp) == TB_THUMBTRACK) is_dragging_track_ = true;
            else if (LOWORD(wp) == TB_ENDTRACK || LOWORD(wp) == TB_THUMBPOSITION) {
                is_dragging_track_ = false;
                auto v = static_cast<std::int64_t>(SendMessageW(track_position_, TBM_GETPOS, 0, 0));
                auto d = coordinator_->GetState().duration_ms;
                if (auto r = coordinator_->Seek(d > 0 ? d * v / 1000 : 0); !r.Ok()) ShowError(r.Error());
                SyncUi();
            }
        } return 0;
    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case IdOpenAudio: { auto p = OpenFileDialog(L"Audio\0*.mp3;*.wav\0All\0*.*\0"); if (!p.empty()) { if (auto r = coordinator_->LoadAudio(p); !r.Ok()) ShowError(r.Error()); SyncUi(); } return 0; }
        case IdOpenSubtitle: { auto p = OpenFileDialog(L"Subtitle\0*.srt;*.lrc\0All\0*.*\0"); if (!p.empty()) { if (auto r = coordinator_->LoadSubtitles(p); !r.Ok()) ShowError(r.Error()); PopulateSubtitleList(); SyncUi(); } return 0; }
        case IdPlay: {
            const auto& state = coordinator_->GetState();
            if (state.player_state == PlayerVisualState::Playing) {
                if (auto r = coordinator_->Pause(); !r.Ok()) ShowError(r.Error());
            } else {
                if (auto r = coordinator_->Play(); !r.Ok()) ShowError(r.Error());
            }
            SyncUi();
            return 0;
        }
        case IdPrevSentence: if (auto r = coordinator_->SelectPreviousSubtitle(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdNextSentence: if (auto r = coordinator_->SelectNextSubtitle(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdSubtitleList:
            if (HIWORD(wp) == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(list_subtitles_, LB_GETCURSEL, 0, 0));
                if (sel != LB_ERR) {
                    const auto& subs = coordinator_->GetState().subtitles;
                    if (sel >= 0 && sel < static_cast<int>(subs.size()))
                        if (auto r = coordinator_->SelectSubtitle(subs[sel].index); !r.Ok()) ShowError(r.Error());
                } SyncUi();
            } return 0;
        case IdModeCombo: if (HIWORD(wp) == CBN_SELCHANGE) { coordinator_->SetPlaybackMode(SelectedPlaybackMode()); SyncUi(); } return 0;
        case IdAutoPauseEdit: if (HIWORD(wp) == EN_CHANGE) { wchar_t b[16]{}; GetWindowTextW(edit_auto_pause_, b, 16); coordinator_->SetAutoPauseDelayMs(_wtoi64(b) * 1000LL); } return 0;
        case IdRecordStart: if (auto r = coordinator_->StartRecording(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdRecordStop: if (auto r = coordinator_->StopRecording(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdPlayOriginal: if (auto r = coordinator_->PlayOriginalSegment(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdPlayRecording: if (auto r = coordinator_->PlayRecording(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        default: break;
        } break;
    }
    case WM_DESTROY: KillTimer(hwnd_, kUiTimerId); PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void MainWindow::CreateControls() {
    font_large_ = CreateFontW(24, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_normal_ = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_small_ = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_caption_ = CreateFontW(13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_mono_ = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Cascadia Mono");

    button_open_audio_ = Btn(hwnd_, instance_, L"Open Audio", IdOpenAudio, 108, 32);
    button_open_subtitle_ = Btn(hwnd_, instance_, L"Open Subtitle", IdOpenSubtitle, 118, 32);
    label_audio_name_ = Lbl(hwnd_, instance_, L"No audio loaded", 280);
    label_subtitle_name_ = Lbl(hwnd_, instance_, L"No subtitle loaded", 280);
    label_time_ = Lbl(hwnd_, instance_, L"00:00 / 00:00", 150);

    label_current_sentence_ = Lbl(hwnd_, instance_, L"Select a subtitle to begin learning", 700);
    SendMessageW(label_current_sentence_, WM_SETFONT, reinterpret_cast<WPARAM>(font_large_), TRUE);

    label_sentence_info_ = Lbl(hwnd_, instance_, L"Current sentence", 240);
    label_sentence_time_ = Lbl(hwnd_, instance_, L"", 320);
    label_practice_ = Lbl(hwnd_, instance_, L"Practice", 100);
    label_hint_ = Lbl(hwnd_, instance_, L"Ready", 520);

    button_play_ = Btn(hwnd_, instance_, L"Play", IdPlay, 110, 36);
    button_prev_sentence_ = Btn(hwnd_, instance_, L"Prev", IdPrevSentence, 78, 36);
    button_next_sentence_ = Btn(hwnd_, instance_, L"Next", IdNextSentence, 78, 36);

    track_position_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_BOTH,
                                      0, 0, 100, 28, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdTrackPosition)), instance_, nullptr);
    SendMessageW(track_position_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));

    button_play_original_ = Btn(hwnd_, instance_, L"Original", IdPlayOriginal, 108, 34);
    button_record_start_ = Btn(hwnd_, instance_, L"Record", IdRecordStart, 108, 34);
    button_record_stop_ = Btn(hwnd_, instance_, L"Stop Rec", IdRecordStop, 108, 34);
    button_play_recording_ = Btn(hwnd_, instance_, L"My Voice", IdPlayRecording, 108, 34);

    combo_mode_ = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                  0, 0, 140, 200, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdModeCombo)), instance_, nullptr);
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Normal"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Repeat One"));
    SendMessageW(combo_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Auto Pause"));
    SendMessageW(combo_mode_, CB_SETCURSEL, 0, 0);

    label_delay_text_ = Lbl(hwnd_, instance_, L"Pause(s):", 60);
    edit_auto_pause_ = Edt(hwnd_, instance_, L"1", IdAutoPauseEdit, 45);

    list_subtitles_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTBOXW, L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                      0, 0, 400, 300, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdSubtitleList)), instance_, nullptr);

    auto set_font = [&](HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); };
    set_font(label_audio_name_, font_caption_); set_font(label_subtitle_name_, font_caption_);
    set_font(label_time_, font_mono_); set_font(label_sentence_info_, font_normal_);
    set_font(label_sentence_time_, font_small_);
    set_font(label_practice_, font_small_);
    set_font(label_hint_, font_small_); set_font(label_delay_text_, font_caption_);
    set_font(button_open_audio_, font_caption_);
    set_font(button_open_subtitle_, font_caption_);
    set_font(button_play_, font_caption_);
    set_font(button_prev_sentence_, font_caption_);
    set_font(button_next_sentence_, font_caption_);
    set_font(button_play_original_, font_caption_);
    set_font(button_record_start_, font_caption_);
    set_font(button_record_stop_, font_caption_);
    set_font(button_play_recording_, font_caption_);
    set_font(combo_mode_, font_caption_);
    set_font(edit_auto_pause_, font_caption_);
    set_font(list_subtitles_, font_normal_);
}

void MainWindow::LayoutControls(int width, int height) {
    const int margin = 16;
    const int gutter = 12;
    const int right_width = (width > 1120) ? 350 : 310;
    const int left_width = width - margin * 2 - right_width - gutter;
    const int safe_left = left_width < 480 ? 480 : left_width;

    const int top_h = 94;
    const int sentence_h = 120;
    const int left_x = margin;
    const int right_x = margin + safe_left + gutter;
    const int top_y = margin;

    MoveWindow(button_open_audio_, left_x, top_y, 108, 30, TRUE);
    MoveWindow(button_open_subtitle_, left_x + 114, top_y, 118, 30, TRUE);
    MoveWindow(label_audio_name_, left_x + 242, top_y + 3, safe_left - 250, 18, TRUE);
    MoveWindow(label_subtitle_name_, left_x + 242, top_y + 23, safe_left - 250, 18, TRUE);
    MoveWindow(label_time_, left_x + safe_left - 175, top_y + 1, 170, 22, TRUE);
    MoveWindow(track_position_, left_x, top_y + 54, safe_left, 26, TRUE);

    const int sentence_y = top_y + top_h + gutter;
    MoveWindow(label_current_sentence_, left_x, sentence_y, safe_left, 60, TRUE);
    MoveWindow(label_sentence_info_, left_x, sentence_y + 66, 170, 20, TRUE);
    MoveWindow(label_sentence_time_, left_x + 170, sentence_y + 66, safe_left - 170, 20, TRUE);

    const int control_y = sentence_y + sentence_h + gutter;
    const int main_y = control_y + 4;
    int main_x = left_x;
    MoveWindow(button_prev_sentence_, main_x, main_y, 92, 36, TRUE); main_x += 98;
    MoveWindow(button_play_, main_x, main_y, 110, 36, TRUE); main_x += 116;
    MoveWindow(button_next_sentence_, main_x, main_y, 92, 36, TRUE);

    const int practice_y = main_y + 46;
    MoveWindow(label_practice_, left_x, practice_y + 8, 62, 18, TRUE);
    int practice_x = left_x + 66;
    MoveWindow(button_play_original_, practice_x, practice_y, 88, 30, TRUE); practice_x += 94;
    MoveWindow(button_record_start_, practice_x, practice_y, 88, 30, TRUE); practice_x += 94;
    MoveWindow(button_record_stop_, practice_x, practice_y, 88, 30, TRUE); practice_x += 94;
    MoveWindow(button_play_recording_, practice_x, practice_y, 88, 30, TRUE);

    MoveWindow(label_hint_, left_x, practice_y + 34, safe_left, 18, TRUE);

    MoveWindow(combo_mode_, right_x, top_y + 2, right_width - 88, 220, TRUE);
    MoveWindow(label_delay_text_, right_x + right_width - 82, top_y + 6, 56, 18, TRUE);
    MoveWindow(edit_auto_pause_, right_x + right_width - 56, top_y + 24, 52, 24, TRUE);
    MoveWindow(list_subtitles_, right_x, top_y + 56, right_width, height - margin - (top_y + 56), TRUE);
}

void MainWindow::OnTimer() { coordinator_->Tick(); SyncUi(); }

void MainWindow::SyncUi() {
    const auto& s = coordinator_->GetState();
    SetWindowTextW(label_time_, (FormatTimestamp(s.position_ms) + L" / " + FormatTimestamp(s.duration_ms)).c_str());
    SetWindowTextW(label_audio_name_, s.audio_display_name.c_str());
    SetWindowTextW(label_subtitle_name_, s.subtitle_display_name.c_str());

    if (!s.current_sentence_text.empty()) {
        SetWindowTextW(label_current_sentence_, s.current_sentence_text.c_str());
        if (s.current_subtitle_index.has_value()) {
            SetWindowTextW(label_sentence_info_, (L"Sentence #" + std::to_wstring(*s.current_subtitle_index)).c_str());
        } else {
            SetWindowTextW(label_sentence_info_, L"Current sentence");
        }
    } else {
        SetWindowTextW(label_current_sentence_, L"Select a subtitle to begin learning");
        SetWindowTextW(label_sentence_info_, L"Current sentence");
    }
    if (s.current_subtitle_index.has_value()) {
        SetWindowTextW(label_sentence_time_, (FormatTimestamp(s.current_sentence_start_ms) + L" \u2192 " + FormatTimestamp(s.current_sentence_end_ms)).c_str());
    } else {
        SetWindowTextW(label_sentence_time_, L"--:--.--- \u2192 --:--.---");
    }

    SetWindowTextW(label_hint_, BuildHintText(s).c_str());

    if (!is_dragging_track_) {
        auto tv = s.duration_ms > 0 ? static_cast<LPARAM>((s.position_ms * 1000) / s.duration_ms) : 0;
        SendMessageW(track_position_, TBM_SETPOS, TRUE, tv);
    }
    int sm = 0;
    switch (s.playback_mode) { case PlaybackMode::RepeatOne: sm = 1; break; case PlaybackMode::AutoPause: sm = 2; break; default: sm = 0; }
    SendMessageW(combo_mode_, CB_SETCURSEL, sm, 0);

    auto ps = std::to_wstring(s.auto_pause_delay_ms / 1000);
    wchar_t ct[16]{}; GetWindowTextW(edit_auto_pause_, ct, 16);
    if (ps != ct) SetWindowTextW(edit_auto_pause_, ps.c_str());

    if (s.player_state == PlayerVisualState::Playing) {
        SetWindowTextW(button_play_, L"Pause");
        EnableWindow(button_play_, s.can_pause);
    } else {
        SetWindowTextW(button_play_, L"Play");
        EnableWindow(button_play_, s.can_play);
    }
    EnableWindow(button_prev_sentence_, s.can_go_previous_sentence);
    EnableWindow(button_next_sentence_, s.can_go_next_sentence); EnableWindow(button_play_original_, s.can_play_original_segment);
    EnableWindow(button_record_start_, s.can_start_recording); EnableWindow(button_record_stop_, s.can_stop_recording);
    EnableWindow(button_play_recording_, s.can_play_recording);

    if (s.current_subtitle_index.has_value())
        for (int i = 0; i < static_cast<int>(s.subtitles.size()); ++i)
            if (s.subtitles[i].index == *s.current_subtitle_index) { SendMessageW(list_subtitles_, LB_SETCURSEL, i, 0); break; }
}

void MainWindow::PopulateSubtitleList() {
    SendMessageW(list_subtitles_, LB_RESETCONTENT, 0, 0);
    for (const auto& line : coordinator_->GetState().subtitles)
        SendMessageW(list_subtitles_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>((FormatTimePrefix(line.start_ms) + L"  " + line.text).c_str()));
}

void MainWindow::ShowError(const AppError& e) { MessageBoxW(hwnd_, e.message.c_str(), kMainWindowTitle, MB_OK | MB_ICONERROR); }

std::wstring MainWindow::OpenFileDialog(const wchar_t* filter) const {
    wchar_t fp[MAX_PATH]{}; OPENFILENAMEW of{};
    of.lStructSize = sizeof(of); of.hwndOwner = hwnd_; of.lpstrFilter = filter; of.lpstrFile = fp; of.nMaxFile = MAX_PATH; of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&of) ? std::wstring(fp) : L"";
}

PlaybackMode MainWindow::SelectedPlaybackMode() const {
    switch (static_cast<int>(SendMessageW(combo_mode_, CB_GETCURSEL, 0, 0))) { case 1: return PlaybackMode::RepeatOne; case 2: return PlaybackMode::AutoPause; default: return PlaybackMode::Normal; }
}

std::wstring MainWindow::FormatTimePrefix(std::int64_t ms) const {
    wchar_t buf[32]{};
    swprintf_s(buf, L"%02d:%02d.%03d", static_cast<int>(ms / 60000), static_cast<int>((ms / 1000) % 60), static_cast<int>(ms % 1000));
    return buf;
}

std::wstring MainWindow::BuildHintText(const AppState& state) const {
    if (!state.hint_text.empty()) return state.hint_text;
    return L"Mode: " + state.mode_status_text + L" | Recording: " + state.recording_status_text;
}

} // namespace replayer
