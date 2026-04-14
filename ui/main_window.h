#pragma once

#include <memory>
#include <optional>
#include <windows.h>

#include "core/playback_coordinator.h"

namespace replayer {

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();

    Result<void> Create(int cmd_show);
    HWND Handle() const noexcept;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);

    void CreateControls();
    void LayoutControls(int width, int height);
    void OnTimer();
    void SyncUi();
    void PopulateSubtitleList();
    void InvalidateSubtitleRow(int row) const;
    int FindSubtitleRowByIndex(std::optional<int> subtitle_index) const;
    int FindLastPlayedSubtitleRow(std::int64_t position_ms) const;
    int MeasureSubtitleItemHeight(std::size_t list_index) const;
    void DrawSubtitleItem(const DRAWITEMSTRUCT& dis);
    void ShowError(const AppError& error);
    std::wstring OpenFileDialog(const wchar_t* filter) const;
    PlaybackMode SelectedPlaybackMode() const;
    std::wstring FormatTimePrefix(std::int64_t ms) const;

    static LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    void DrawModernButton(HWND hwnd, HDC hdc, bool hovered, bool pressed, bool disabled, COLORREF baseColor);
    void DrawRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF border);
    void DrawSurface(HDC hdc);
    void DrawHeaderPanel(HDC hdc, const RECT& rect);
    void DrawCard(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius);
    void UpdateHoverState(int x, int y);

    HINSTANCE instance_;
    HWND hwnd_{nullptr};

    HWND button_open_audio_{nullptr};
    HWND button_open_subtitle_{nullptr};
    HWND label_audio_name_{nullptr};
    HWND label_subtitle_name_{nullptr};
    HWND label_time_{nullptr};

    HWND label_current_sentence_{nullptr};
    HWND label_sentence_info_{nullptr};
    HWND label_sentence_index_{nullptr};
    HWND label_sentence_time_{nullptr};
    HWND label_mode_{nullptr};
    HWND label_recording_status_{nullptr};
    HWND label_hint_{nullptr};

    HWND button_play_{nullptr};
    HWND button_pause_{nullptr};
    HWND button_stop_{nullptr};
    HWND button_prev_sentence_{nullptr};
    HWND button_next_sentence_{nullptr};
    HWND button_play_original_{nullptr};
    HWND button_record_start_{nullptr};
    HWND button_record_stop_{nullptr};
    HWND button_play_recording_{nullptr};
    HWND track_position_{nullptr};
    HWND combo_mode_{nullptr};
    HWND label_delay_text_{nullptr};
    HWND edit_auto_pause_{nullptr};
    HWND list_subtitles_{nullptr};

    HFONT font_large_{nullptr};
    HFONT font_normal_{nullptr};
    HFONT font_small_{nullptr};
    HFONT font_bold_{nullptr};
    HFONT font_mono_{nullptr};
    HFONT font_title_{nullptr};
    HFONT font_caption_{nullptr};
    HBRUSH brush_card_{nullptr};

    bool is_dragging_track_{false};
    HWND hovered_button_{nullptr};
    RECT rect_header_{};
    RECT rect_sentence_card_{};
    RECT rect_controls_card_{};
    RECT rect_subtitle_card_{};
    std::optional<int> last_current_subtitle_row_{};
    std::optional<int> last_played_subtitle_row_{};
    int subtitle_line_height_{20};
    std::unique_ptr<PlaybackCoordinator> coordinator_;
};

} // namespace replayer
