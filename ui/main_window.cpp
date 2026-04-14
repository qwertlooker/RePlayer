#include "ui/main_window.h"

#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

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
    IdOpenAudio = 2001, IdOpenSubtitle, IdPlay, IdPause, IdStop, IdTrackPosition,
    IdSubtitleList, IdModeCombo, IdAutoPauseEdit, IdRecordStart, IdRecordStop,
    IdPlayOriginal, IdPlayRecording, IdPrevSentence, IdNextSentence
};

constexpr COLORREF C_BG_START      = RGB(245, 248, 255);
constexpr COLORREF C_BG_END        = RGB(235, 241, 252);
constexpr COLORREF C_HEADER_FILL   = RGB(255, 255, 255);
constexpr COLORREF C_CARD          = RGB(255, 255, 255);
constexpr COLORREF C_CARD_H        = RGB(250, 252, 255);
constexpr COLORREF C_BORDER        = RGB(216, 226, 242);
constexpr COLORREF C_TEXT          = RGB(30, 41, 59);
constexpr COLORREF C_TEXT2         = RGB(71, 85, 105);
constexpr COLORREF C_TEXT3         = RGB(100, 116, 139);
constexpr COLORREF C_PRIMARY       = RGB(37, 99, 235);
constexpr COLORREF C_SUCCESS       = RGB(22, 163, 74);
constexpr COLORREF C_DANGER        = RGB(220, 38, 38);
constexpr COLORREF C_SECONDARY     = RGB(148, 163, 184);
constexpr COLORREF C_ACCENT_B      = RGB(29, 78, 216);
constexpr COLORREF C_ACCENT_G      = RGB(21, 128, 61);
constexpr COLORREF C_ACCENT_R      = RGB(185, 28, 28);
HWND Btn(HWND p, HINSTANCE i, const wchar_t* t, int id, int w = 90, int h = 32) {
    return CreateWindowExW(0, WC_BUTTONW, t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_FLAT,
                           0, 0, w, h, p, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), i, nullptr);
}

HWND Lbl(HWND p, HINSTANCE i, const wchar_t* t, int w = 200) {
    return CreateWindowExW(0, WC_STATICW, t, WS_CHILD | WS_VISIBLE, 0, 0, w, 22, p, nullptr, i, nullptr);
}

HWND Edt(HWND p, HINSTANCE i, const wchar_t* t, int id, int w = 60) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, t, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER | ES_NUMBER,
                           0, 0, w, 28, p, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), i, nullptr);
}

COLORREF Darken(COLORREF c, float f) {
    return RGB(static_cast<int>(GetRValue(c) * f), static_cast<int>(GetGValue(c) * f), static_cast<int>(GetBValue(c) * f));
}

} // namespace

MainWindow::MainWindow(HINSTANCE instance) : instance_(instance) {}
MainWindow::~MainWindow() {
    if (font_large_) DeleteObject(font_large_);
    if (font_normal_) DeleteObject(font_normal_);
    if (font_small_) DeleteObject(font_small_);
    if (font_bold_) DeleteObject(font_bold_);
    if (font_mono_) DeleteObject(font_mono_);
    if (font_title_) DeleteObject(font_title_);
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
    wc.hbrBackground = CreateSolidBrush(C_BG_START);

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
        InvalidateRect(hwnd_, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);
        DrawSurface(hdc);
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        HWND ctl = reinterpret_cast<HWND>(lp);
        SetBkMode(hdc, TRANSPARENT);
        if (ctl == label_current_sentence_) { SetTextColor(hdc, C_ACCENT_B); return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH)); }
        if (ctl == label_time_) { SetTextColor(hdc, C_TEXT); return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH)); }
        if (ctl == label_sentence_info_ || ctl == label_sentence_index_ || ctl == label_sentence_time_) { SetTextColor(hdc, C_TEXT2); return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH)); }
        if (ctl == label_mode_) { SetTextColor(hdc, C_PRIMARY); return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH)); }
        if (ctl == label_recording_status_) {
            const auto& s = coordinator_->GetState();
            SetTextColor(hdc, s.is_recording ? C_ACCENT_R : (s.has_current_sentence_recording ? C_ACCENT_G : C_TEXT3));
            return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
        }
        SetTextColor(hdc, (ctl == label_hint_ || ctl == label_audio_name_ || ctl == label_subtitle_name_ || ctl == label_delay_text_) ? C_TEXT3 : C_TEXT2);
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wp);
        SetBkColor(hdc, C_CARD_H); SetTextColor(hdc, C_TEXT);
        return reinterpret_cast<LRESULT>(CreateSolidBrush(C_CARD));
    }
    case WM_MOUSEMOVE: UpdateHoverState(GET_X_LPARAM(wp), GET_Y_LPARAM(lp)); break;
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
        case IdPlay: if (auto r = coordinator_->Play(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdPause: if (auto r = coordinator_->Pause(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
        case IdStop: if (auto r = coordinator_->Stop(); !r.Ok()) ShowError(r.Error()); SyncUi(); return 0;
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
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (dis->CtlType == ODT_BUTTON) {
            bool hv = (dis->hwndItem == hovered_button_), pr = (dis->itemState & ODS_SELECTED), ds = (dis->itemState & ODS_DISABLED);
            COLORREF bc = C_SECONDARY;
            if (dis->hwndItem == button_play_ || dis->hwndItem == button_prev_sentence_ ||
                dis->hwndItem == button_next_sentence_ || dis->hwndItem == button_play_original_) bc = C_PRIMARY;
            else if (dis->hwndItem == button_record_start_) bc = C_DANGER;
            else if (dis->hwndItem == button_record_stop_ || dis->hwndItem == button_play_recording_) bc = C_SUCCESS;
            DrawModernButton(dis->hwndItem, dis->hDC, hv, pr, ds, bc);
            return TRUE;
        } break;
    }
    case WM_DESTROY: KillTimer(hwnd_, kUiTimerId); PostQuitMessage(0); return 0;
    default: break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

void MainWindow::CreateControls() {
    font_title_ = CreateFontW(30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_large_ = CreateFontW(26, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_normal_ = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_small_ = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_caption_ = CreateFontW(13, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_bold_ = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    font_mono_ = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Cascadia Mono");

    button_open_audio_ = Btn(hwnd_, instance_, L"Open Audio", IdOpenAudio, 108, 32);
    button_open_subtitle_ = Btn(hwnd_, instance_, L"Open Subtitle", IdOpenSubtitle, 118, 32);
    label_audio_name_ = Lbl(hwnd_, instance_, L"No audio loaded", 280);
    label_subtitle_name_ = Lbl(hwnd_, instance_, L"No subtitle loaded", 280);
    label_time_ = Lbl(hwnd_, instance_, L"00:00 / 00:00", 150);

    label_current_sentence_ = Lbl(hwnd_, instance_, L"Select a subtitle to begin learning", 700);
    SendMessageW(label_current_sentence_, WM_SETFONT, reinterpret_cast<WPARAM>(font_large_), TRUE);

    label_sentence_info_ = Lbl(hwnd_, instance_, L"", 600);
    label_sentence_index_ = Lbl(hwnd_, instance_, L"", 110);
    label_sentence_time_ = Lbl(hwnd_, instance_, L"", 180);
    label_mode_ = Lbl(hwnd_, instance_, L"Mode: Normal", 130);
    label_recording_status_ = Lbl(hwnd_, instance_, L"No recording", 140);
    label_hint_ = Lbl(hwnd_, instance_, L"", 350);

    button_play_ = Btn(hwnd_, instance_, L"Play", IdPlay, 92, 36);
    button_pause_ = Btn(hwnd_, instance_, L"Pause", IdPause, 92, 36);
    button_stop_ = Btn(hwnd_, instance_, L"Stop", IdStop, 92, 36);
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

    for (auto b : {button_open_audio_, button_open_subtitle_, button_play_, button_pause_,
                   button_stop_, button_prev_sentence_, button_next_sentence_, button_play_original_,
                   button_record_start_, button_record_stop_, button_play_recording_})
        SetWindowSubclass(b, ButtonProc, 0, reinterpret_cast<DWORD_PTR>(this));

    auto set_font = [&](HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); };
    set_font(label_audio_name_, font_caption_); set_font(label_subtitle_name_, font_caption_);
    set_font(label_time_, font_mono_); set_font(label_sentence_info_, font_normal_);
    set_font(label_sentence_time_, font_small_); set_font(label_sentence_index_, font_small_);
    set_font(label_mode_, font_bold_); set_font(label_recording_status_, font_bold_);
    set_font(label_hint_, font_small_); set_font(label_delay_text_, font_caption_);
    set_font(button_open_audio_, font_caption_);
    set_font(button_open_subtitle_, font_caption_);
    set_font(button_play_, font_caption_);
    set_font(button_pause_, font_caption_);
    set_font(button_stop_, font_caption_);
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
    const int gutter = 14;
    const int right_width = (width > 1100) ? 420 : 360;
    const int left_width = width - margin * 2 - right_width - gutter;
    const int safe_left = left_width < 480 ? 480 : left_width;

    rect_header_ = {margin, margin, width - margin, margin + 108};
    rect_sentence_card_ = {margin, rect_header_.bottom + gutter, margin + safe_left, rect_header_.bottom + gutter + 172};
    rect_controls_card_ = {margin, rect_sentence_card_.bottom + gutter, margin + safe_left, rect_sentence_card_.bottom + gutter + 128};
    rect_subtitle_card_ = {margin + safe_left + gutter, rect_header_.bottom + gutter, width - margin, height - margin};

    const int hx = rect_header_.left + 20;
    const int hy = rect_header_.top + 18;
    MoveWindow(button_open_audio_, hx, hy + 44, 108, 32, TRUE);
    MoveWindow(button_open_subtitle_, hx + 118, hy + 44, 118, 32, TRUE);
    MoveWindow(label_audio_name_, hx + 250, hy + 48, 270, 20, TRUE);
    MoveWindow(label_subtitle_name_, hx + 250, hy + 72, 270, 20, TRUE);
    MoveWindow(track_position_, rect_header_.left + 22, rect_header_.bottom - 32, rect_header_.right - rect_header_.left - 44, 22, TRUE);
    MoveWindow(label_time_, rect_header_.right - 196, rect_header_.top + 22, 166, 26, TRUE);

    MoveWindow(label_current_sentence_, rect_sentence_card_.left + 20, rect_sentence_card_.top + 20, rect_sentence_card_.right - rect_sentence_card_.left - 36, 68, TRUE);
    MoveWindow(label_sentence_info_, rect_sentence_card_.left + 20, rect_sentence_card_.top + 96, rect_sentence_card_.right - rect_sentence_card_.left - 36, 24, TRUE);
    MoveWindow(label_sentence_index_, rect_sentence_card_.left + 20, rect_sentence_card_.top + 126, 120, 20, TRUE);
    MoveWindow(label_sentence_time_, rect_sentence_card_.left + 152, rect_sentence_card_.top + 126, 188, 20, TRUE);
    MoveWindow(label_mode_, rect_sentence_card_.left + 352, rect_sentence_card_.top + 126, 160, 20, TRUE);
    MoveWindow(label_recording_status_, rect_sentence_card_.left + 520, rect_sentence_card_.top + 126, 160, 20, TRUE);

    const int by = rect_controls_card_.top + 26;
    int bx = rect_controls_card_.left + 18;
    MoveWindow(button_play_, bx, by, 92, 36, TRUE); bx += 98;
    MoveWindow(button_pause_, bx, by, 92, 36, TRUE); bx += 98;
    MoveWindow(button_stop_, bx, by, 92, 36, TRUE); bx += 100;
    MoveWindow(button_prev_sentence_, bx, by, 78, 36, TRUE); bx += 84;
    MoveWindow(button_next_sentence_, bx, by, 78, 36, TRUE);

    int tx = rect_controls_card_.left + 18;
    const int row2y = by + 44;
    MoveWindow(button_play_original_, tx, row2y, 108, 34, TRUE); tx += 114;
    MoveWindow(button_record_start_, tx, row2y, 108, 34, TRUE); tx += 114;
    MoveWindow(button_record_stop_, tx, row2y, 108, 34, TRUE); tx += 114;
    MoveWindow(button_play_recording_, tx, row2y, 108, 34, TRUE);

    MoveWindow(combo_mode_, rect_controls_card_.right - 168, by + 2, 144, 200, TRUE);
    MoveWindow(label_delay_text_, rect_controls_card_.right - 166, row2y + 8, 70, 18, TRUE);
    MoveWindow(edit_auto_pause_, rect_controls_card_.right - 84, row2y + 4, 60, 28, TRUE);
    MoveWindow(label_hint_, rect_controls_card_.left + 20, rect_controls_card_.bottom - 26, rect_controls_card_.right - rect_controls_card_.left - 40, 18, TRUE);

    MoveWindow(list_subtitles_, rect_subtitle_card_.left + 14, rect_subtitle_card_.top + 54,
               rect_subtitle_card_.right - rect_subtitle_card_.left - 28, rect_subtitle_card_.bottom - rect_subtitle_card_.top - 68, TRUE);
}

void MainWindow::OnTimer() { coordinator_->Tick(); SyncUi(); }

void MainWindow::SyncUi() {
    const auto& s = coordinator_->GetState();
    SetWindowTextW(label_time_, (FormatTimestamp(s.position_ms) + L" / " + FormatTimestamp(s.duration_ms)).c_str());
    SetWindowTextW(label_audio_name_, s.audio_display_name.c_str());
    SetWindowTextW(label_subtitle_name_, s.subtitle_display_name.c_str());

    if (!s.current_sentence_text.empty()) {
        SetWindowTextW(label_current_sentence_, s.current_sentence_text.c_str());
        std::wstring info;
        if (s.current_subtitle_index.has_value())
            info = L"Sentence #" + std::to_wstring(*s.current_subtitle_index) +
                   L"   \u2022   " + FormatTimestamp(s.current_sentence_start_ms) +
                   L" \u2192 " + FormatTimestamp(s.current_sentence_end_ms);
        SetWindowTextW(label_sentence_info_, info.c_str());
    } else {
        SetWindowTextW(label_current_sentence_, L"Select a subtitle to begin learning");
        SetWindowTextW(label_sentence_info_, L"");
    }
    if (s.current_subtitle_index.has_value()) {
        SetWindowTextW(label_sentence_index_, (L"#" + std::to_wstring(*s.current_subtitle_index)).c_str());
        SetWindowTextW(label_sentence_time_, (FormatTimestamp(s.current_sentence_start_ms) + L" \u2192 " + FormatTimestamp(s.current_sentence_end_ms)).c_str());
    } else { SetWindowTextW(label_sentence_index_, L""); SetWindowTextW(label_sentence_time_, L""); }

    SetWindowTextW(label_mode_, (L"Mode: " + s.mode_status_text).c_str());
    SetWindowTextW(label_recording_status_, s.recording_status_text.c_str());
    SetWindowTextW(label_hint_, s.hint_text.c_str());
    InvalidateRect(label_recording_status_, nullptr, FALSE);

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

    EnableWindow(button_play_, s.can_play); EnableWindow(button_pause_, s.can_pause);
    EnableWindow(button_stop_, s.can_stop); EnableWindow(button_prev_sentence_, s.can_go_previous_sentence);
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

LRESULT CALLBACK MainWindow::ButtonProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    if (msg == WM_MOUSEMOVE) reinterpret_cast<MainWindow*>(data)->UpdateHoverState(GET_X_LPARAM(wp), GET_Y_LPARAM(lp));
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void MainWindow::UpdateHoverState(int x, int y) {
    POINT pt{x, y}; ClientToScreen(hwnd_, &pt); ScreenToClient(hwnd_, &pt);
    HWND under = ChildWindowFromPoint(hwnd_, pt);
    HWND prev = hovered_button_;
    hovered_button_ = (under && GetParent(under) == hwnd_) ? under : nullptr;
    if (prev && prev != hovered_button_) InvalidateRect(prev, nullptr, TRUE);
    if (hovered_button_ && hovered_button_ != prev) InvalidateRect(hovered_button_, nullptr, TRUE);
}

void MainWindow::DrawModernButton(HWND hwnd, HDC hdc, bool hovered, bool pressed, bool disabled, COLORREF baseColor) {
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    COLORREF fill = disabled ? RGB(226, 232, 240) : (pressed ? Darken(baseColor, 0.88f) : (hovered ? baseColor : C_CARD));
    COLORREF border = disabled ? RGB(203, 213, 225) : (pressed ? Darken(baseColor, 0.88f) : (hovered ? baseColor : C_BORDER));

    DrawRoundRect(hdc, 0, 0, w, h, 6, fill, border);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? RGB(148, 163, 184) : (hovered || pressed ? RGB(255, 255, 255) : C_TEXT));
    wchar_t text[64]{}; GetWindowTextW(hwnd, text, 64);

    HFONT old_font = reinterpret_cast<HFONT>(SelectObject(hdc, font_caption_));
    RECT tr{2, 0, w - 2, h};
    DrawTextW(hdc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old_font);
}

void MainWindow::DrawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border) {
    HRGN rgn = CreateRoundRectRgn(x, y, x + w + 1, y + h + 1, r, r);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH old_brush = reinterpret_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN old_pen = reinterpret_cast<HPEN>(SelectObject(hdc, pen));
    PaintRgn(hdc, rgn);
    HBRUSH border_brush = CreateSolidBrush(border);
    FrameRgn(hdc, rgn, border_brush, 1, 1);
    DeleteObject(border_brush);
    SelectObject(hdc, old_pen); SelectObject(hdc, old_brush);
    DeleteObject(pen); DeleteObject(brush); DeleteObject(rgn);
}

void MainWindow::DrawSurface(HDC hdc) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    HBRUSH bg_brush = CreateSolidBrush(C_BG_END);
    FillRect(hdc, &client, bg_brush);
    DeleteObject(bg_brush);

    DrawHeaderPanel(hdc, rect_header_);
    DrawCard(hdc, rect_sentence_card_, C_CARD, C_BORDER, 18);
    DrawCard(hdc, rect_controls_card_, C_CARD, C_BORDER, 18);
    DrawCard(hdc, rect_subtitle_card_, C_CARD, C_BORDER, 18);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, C_TEXT);
    HFONT old_font = reinterpret_cast<HFONT>(SelectObject(hdc, font_title_));
    RECT title_rect{rect_header_.left + 20, rect_header_.top + 12, rect_header_.right - 200, rect_header_.top + 50};
    DrawTextW(hdc, L"RePlayer", -1, &title_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, font_caption_);
    SetTextColor(hdc, C_TEXT2);
    RECT subtitle_rect{rect_header_.left + 22, rect_header_.top + 50, rect_header_.right - 210, rect_header_.top + 76};
    DrawTextW(hdc, L"Win11 style English listening studio", -1, &subtitle_rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SetTextColor(hdc, C_TEXT2);
    RECT list_title{rect_subtitle_card_.left + 18, rect_subtitle_card_.top + 16, rect_subtitle_card_.right - 18, rect_subtitle_card_.top + 40};
    DrawTextW(hdc, L"Subtitles", -1, &list_title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, old_font);
}

void MainWindow::DrawHeaderPanel(HDC hdc, const RECT& rect) {
    DrawCard(hdc, rect, C_HEADER_FILL, C_BORDER, 20);
}

void MainWindow::DrawCard(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
    DrawRoundRect(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, radius, fill, border);
}

} // namespace replayer
