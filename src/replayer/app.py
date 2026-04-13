from __future__ import annotations

import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import simpleaudio as sa

from .audio import AudioController
from .models import SubtitleLine
from .recording import Recorder
from .subtitles import load_subtitle_file


class RePlayerApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("RePlayer - English Study")
        self.root.geometry("980x680")

        self.audio = AudioController()
        self.lines: list[SubtitleLine] = []
        self.current_line_idx = -1
        self.repeat_single = tk.BooleanVar(value=False)
        self.auto_pause_ms = tk.IntVar(value=1000)
        self.pause_until_ms = -1
        self.clip_end_ms = -1
        self.audio_loaded = False
        self.recording_dir = Path.cwd() / "recordings"
        self.recorder = Recorder(self.recording_dir)
        self.recording_files: dict[int, Path] = {}

        self._build_ui()
        self._tick()

    def _build_ui(self) -> None:
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill=tk.X)

        ttk.Button(top, text="打开 MP3", command=self.open_mp3).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="加载字幕", command=self.open_subtitle).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="播放/暂停", command=self.toggle_play).pack(side=tk.LEFT, padx=4)

        ttk.Checkbutton(top, text="单句复读", variable=self.repeat_single).pack(side=tk.LEFT, padx=8)

        ttk.Label(top, text="句间暂停(ms)").pack(side=tk.LEFT, padx=(16, 4))
        ttk.Spinbox(top, from_=0, to=10000, increment=100, textvariable=self.auto_pause_ms, width=8).pack(side=tk.LEFT)

        rec = ttk.Frame(self.root, padding=(10, 0))
        rec.pack(fill=tk.X)
        ttk.Button(rec, text="开始录音(当前句)", command=self.start_recording).pack(side=tk.LEFT, padx=4)
        ttk.Button(rec, text="停止录音", command=self.stop_recording).pack(side=tk.LEFT, padx=4)
        ttk.Button(rec, text="播放当前句原音", command=self.play_original_line).pack(side=tk.LEFT, padx=4)
        ttk.Button(rec, text="播放当前句录音", command=self.play_recorded_line).pack(side=tk.LEFT, padx=4)

        self.status_var = tk.StringVar(value="请先打开 MP3 与字幕")
        ttk.Label(self.root, textvariable=self.status_var, padding=(10, 6)).pack(fill=tk.X)

        body = ttk.Frame(self.root, padding=10)
        body.pack(fill=tk.BOTH, expand=True)

        self.listbox = tk.Listbox(body, font=("Segoe UI", 11))
        self.listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.listbox.bind("<<ListboxSelect>>", self.on_list_select)

        scrollbar = ttk.Scrollbar(body, orient=tk.VERTICAL, command=self.listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.listbox.configure(yscrollcommand=scrollbar.set)

    def open_mp3(self) -> None:
        file_path = filedialog.askopenfilename(filetypes=[("MP3 Files", "*.mp3")])
        if not file_path:
            return
        self.audio.load(file_path)
        self.audio_loaded = True
        self.status_var.set(f"已加载音频: {Path(file_path).name}")

    def open_subtitle(self) -> None:
        file_path = filedialog.askopenfilename(filetypes=[("Subtitle", "*.srt *.lrc")])
        if not file_path:
            return
        try:
            self.lines = load_subtitle_file(file_path)
        except Exception as exc:
            messagebox.showerror("字幕加载失败", str(exc))
            return
        self.listbox.delete(0, tk.END)
        for line in self.lines:
            label = self._fmt_ms(line.start_ms)
            self.listbox.insert(tk.END, f"[{label}] {line.text}")
        self.status_var.set(f"已加载字幕: {Path(file_path).name}, 共 {len(self.lines)} 句")

    def toggle_play(self) -> None:
        if not self.audio_loaded:
            messagebox.showwarning("提示", "请先打开 MP3")
            return
        if self.audio.is_playing():
            self.audio.pause()
            self.status_var.set("已暂停")
        else:
            self.audio.play()
            self.status_var.set("播放中")

    def on_list_select(self, _event=None) -> None:
        selected = self.listbox.curselection()
        if not selected or not self.audio_loaded:
            return
        idx = selected[0]
        self.seek_to_line(idx)

    def seek_to_line(self, idx: int) -> None:
        if idx < 0 or idx >= len(self.lines):
            return
        line = self.lines[idx]
        self.audio.seek_ms(line.start_ms)
        self.audio.play()
        self.current_line_idx = idx
        self._highlight(idx)
        self.status_var.set(f"跳转到第 {idx + 1} 句")

    def _highlight(self, idx: int) -> None:
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self.listbox.activate(idx)
        self.listbox.see(idx)

    def _tick(self) -> None:
        try:
            self._refresh_playback_state()
        finally:
            self.root.after(80, self._tick)

    def _refresh_playback_state(self) -> None:
        if not self.audio_loaded or not self.lines:
            return

        now_ms = self.audio.get_time_ms()

        if self.pause_until_ms > 0 and now_ms < self.pause_until_ms:
            self.audio.pause()
            return
        if self.pause_until_ms > 0 and now_ms >= self.pause_until_ms:
            self.pause_until_ms = -1
            self.audio.play()

        if self.clip_end_ms > 0 and now_ms >= self.clip_end_ms:
            self.audio.pause()
            self.clip_end_ms = -1

        idx = self._find_current_line(now_ms)
        if idx != -1 and idx != self.current_line_idx:
            self.current_line_idx = idx
            self._highlight(idx)

        if self.current_line_idx != -1:
            line = self.lines[self.current_line_idx]
            if now_ms >= line.end_ms and self.audio.is_playing():
                if self.repeat_single.get():
                    self.audio.seek_ms(line.start_ms)
                    self.audio.play()
                else:
                    pause_ms = max(0, self.auto_pause_ms.get())
                    if pause_ms > 0:
                        self.audio.pause()
                        self.pause_until_ms = now_ms + pause_ms
                    if self.current_line_idx + 1 < len(self.lines):
                        next_line = self.lines[self.current_line_idx + 1]
                        self.audio.seek_ms(next_line.start_ms)

    def _find_current_line(self, now_ms: int) -> int:
        for i, line in enumerate(self.lines):
            if line.start_ms <= now_ms < line.end_ms:
                return i
        return -1

    def start_recording(self) -> None:
        if self.current_line_idx < 0:
            messagebox.showwarning("提示", "请先选中或播放某句字幕")
            return
        path = self.recorder.start(self.current_line_idx)
        self.status_var.set(f"录音中: {path.name}")

    def stop_recording(self) -> None:
        if not self.recorder.is_recording():
            return
        self.recorder.stop()
        if self.current_line_idx >= 0 and self.recorder.current_file:
            self.recording_files[self.current_line_idx] = self.recorder.current_file
        self.status_var.set("录音已停止")

    def play_original_line(self) -> None:
        if self.current_line_idx < 0:
            return
        line = self.lines[self.current_line_idx]
        self.audio.seek_ms(line.start_ms)
        self.audio.play()
        self.clip_end_ms = line.end_ms
        self.status_var.set("播放当前句原音")

    def play_recorded_line(self) -> None:
        if self.current_line_idx < 0:
            return
        wav_path = self.recording_files.get(self.current_line_idx)
        if not wav_path or not wav_path.exists():
            messagebox.showwarning("提示", "当前句还没有录音")
            return
        wave_obj = sa.WaveObject.from_wave_file(str(wav_path))
        wave_obj.play()
        self.status_var.set(f"播放录音: {wav_path.name}")

    @staticmethod
    def _fmt_ms(value: int) -> str:
        total_s = value // 1000
        minutes, seconds = divmod(total_s, 60)
        millis = value % 1000
        return f"{minutes:02d}:{seconds:02d}.{millis:03d}"


def main() -> None:
    root = tk.Tk()
    style = ttk.Style(root)
    if "vista" in style.theme_names():
        style.theme_use("vista")
    app = RePlayerApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.stop_recording(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    main()
