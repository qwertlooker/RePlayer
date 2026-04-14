#pragma once

#include <memory>
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
    void ShowError(const AppError& error);
    std::wstring OpenFileDialog(const wchar_t* filter) const;
    PlaybackMode SelectedPlaybackMode() const;
    std::wstring FormatTimePrefix(std::int64_t ms) const;
    std::wstring BuildHintText(const AppState& state) const;

    HINSTANCE instance_;
    HWND hwnd_{nullptr};

    HWND button_open_audio_{nullptr};
    HWND button_open_subtitle_{nullptr};
    HWND label_audio_name_{nullptr};
    HWND label_subtitle_name_{nullptr};
    HWND label_time_{nullptr};

    HWND label_current_sentence_{nullptr};
    HWND label_sentence_info_{nullptr};
    HWND label_sentence_time_{nullptr};
    HWND label_practice_{nullptr};
    HWND label_hint_{nullptr};

    HWND button_play_{nullptr};
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
    HFONT font_mono_{nullptr};
    HFONT font_caption_{nullptr};

    bool is_dragging_track_{false};
    std::unique_ptr<PlaybackCoordinator> coordinator_;
};

} // namespace replayer
