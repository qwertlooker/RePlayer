from __future__ import annotations

import re
from pathlib import Path

from .models import SubtitleLine

SRT_TIME_RE = re.compile(
    r"(?P<h>\d{2}):(?P<m>\d{2}):(?P<s>\d{2}),(?P<ms>\d{3})"
)
LRC_TIME_RE = re.compile(r"\[(?P<m>\d{1,2}):(?P<s>\d{2})(?:\.(?P<cs>\d{1,3}))?\]")


def _to_ms(hours: int, minutes: int, seconds: int, millis: int) -> int:
    return ((hours * 60 + minutes) * 60 + seconds) * 1000 + millis


def _parse_srt_time(raw: str) -> int:
    match = SRT_TIME_RE.fullmatch(raw.strip())
    if not match:
        raise ValueError(f"Invalid SRT time: {raw}")
    return _to_ms(
        int(match.group("h")),
        int(match.group("m")),
        int(match.group("s")),
        int(match.group("ms")),
    )


def parse_srt(content: str) -> list[SubtitleLine]:
    blocks = re.split(r"\n\s*\n", content.strip(), flags=re.MULTILINE)
    lines: list[SubtitleLine] = []
    for idx, block in enumerate(blocks):
        rows = [r.strip("\ufeff") for r in block.splitlines() if r.strip()]
        if len(rows) < 2:
            continue
        timing_line = rows[1] if rows[0].isdigit() else rows[0]
        text_rows = rows[2:] if rows[0].isdigit() else rows[1:]
        if "-->" not in timing_line:
            continue
        start_raw, end_raw = [x.strip() for x in timing_line.split("-->")]
        start_ms = _parse_srt_time(start_raw)
        end_ms = _parse_srt_time(end_raw)
        text = " ".join(text_rows).strip()
        lines.append(SubtitleLine(index=idx, start_ms=start_ms, end_ms=end_ms, text=text))
    return lines


def parse_lrc(content: str) -> list[SubtitleLine]:
    parsed: list[tuple[int, str]] = []
    for raw in content.splitlines():
        matches = list(LRC_TIME_RE.finditer(raw))
        if not matches:
            continue
        text = LRC_TIME_RE.sub("", raw).strip()
        for match in matches:
            minutes = int(match.group("m"))
            seconds = int(match.group("s"))
            centi_raw = match.group("cs") or "0"
            millis = int(centi_raw.ljust(3, "0")[:3])
            start_ms = _to_ms(0, minutes, seconds, millis)
            parsed.append((start_ms, text))

    parsed.sort(key=lambda x: x[0])
    lines: list[SubtitleLine] = []
    for idx, (start_ms, text) in enumerate(parsed):
        if idx + 1 < len(parsed):
            end_ms = parsed[idx + 1][0]
        else:
            end_ms = start_ms + 2000
        lines.append(SubtitleLine(index=idx, start_ms=start_ms, end_ms=end_ms, text=text))
    return lines


def load_subtitle_file(path: str | Path) -> list[SubtitleLine]:
    subtitle_path = Path(path)
    content = subtitle_path.read_text(encoding="utf-8-sig", errors="replace")
    suffix = subtitle_path.suffix.lower()
    if suffix == ".srt":
        return parse_srt(content)
    if suffix == ".lrc":
        return parse_lrc(content)
    raise ValueError("Unsupported subtitle format. Please load .srt or .lrc")
