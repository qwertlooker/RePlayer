from __future__ import annotations

import wave
from pathlib import Path

import sounddevice as sd


class Recorder:
    def __init__(self, output_dir: Path) -> None:
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.sample_rate = 16000
        self.channels = 1
        self._stream: sd.InputStream | None = None
        self._wave: wave.Wave_write | None = None
        self.current_file: Path | None = None

    def start(self, line_index: int) -> Path:
        if self._stream is not None:
            return self.current_file or self.output_dir / f"line_{line_index:04d}.wav"
        self.current_file = self.output_dir / f"line_{line_index:04d}.wav"
        self._wave = wave.open(str(self.current_file), "wb")
        self._wave.setnchannels(self.channels)
        self._wave.setsampwidth(2)
        self._wave.setframerate(self.sample_rate)

        def callback(indata, frames, time, status):
            if status:
                print(status)
            if self._wave is not None:
                self._wave.writeframes(indata.tobytes())

        self._stream = sd.InputStream(
            samplerate=self.sample_rate,
            channels=self.channels,
            dtype="int16",
            callback=callback,
        )
        self._stream.start()
        return self.current_file

    def stop(self) -> None:
        if self._stream is not None:
            self._stream.stop()
            self._stream.close()
            self._stream = None
        if self._wave is not None:
            self._wave.close()
            self._wave = None

    def is_recording(self) -> bool:
        return self._stream is not None
