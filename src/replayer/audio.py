from __future__ import annotations

from pathlib import Path

import vlc


class AudioController:
    def __init__(self) -> None:
        self.instance = vlc.Instance("--no-video")
        self.player = self.instance.media_player_new()
        self.audio_path: Path | None = None

    def load(self, file_path: str) -> None:
        self.audio_path = Path(file_path)
        media = self.instance.media_new(str(self.audio_path))
        self.player.set_media(media)

    def play(self) -> None:
        self.player.play()

    def pause(self) -> None:
        self.player.pause()

    def stop(self) -> None:
        self.player.stop()

    def is_playing(self) -> bool:
        return bool(self.player.is_playing())

    def get_time_ms(self) -> int:
        return max(0, self.player.get_time())

    def seek_ms(self, target_ms: int) -> None:
        self.player.set_time(max(0, int(target_ms)))

    def set_volume(self, value: int) -> None:
        self.player.audio_set_volume(max(0, min(100, value)))
