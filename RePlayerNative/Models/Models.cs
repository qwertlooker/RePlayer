using System;
using System.Collections.Generic;

namespace RePlayer.Models;

public class SubtitleLine
{
    public double Start { get; set; }
    public double End { get; set; }
    public string Text { get; set; } = string.Empty;

    public string StartTimeFormatted => FormatTime(Start);

    private static string FormatTime(double seconds)
    {
        seconds = Math.Max(0, seconds);
        var m = (int)(seconds / 60);
        var s = (int)(seconds % 60);
        return $"{m:D2}:{s:D2}";
    }
}

public class Track
{
    public string Name { get; set; } = string.Empty;
    public string Mp3Path { get; set; } = string.Empty;
    public string? SubtitlePath { get; set; }
    public List<SubtitleLine> Subtitles { get; set; } = new();
}
