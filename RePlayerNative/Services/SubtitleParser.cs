using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using RePlayer.Models;

namespace RePlayer.Services;

public static partial class SubtitleParser
{
    public static List<SubtitleLine> ParseSrt(string text)
    {
        var subs = new List<SubtitleLine>();
        var blocks = SrtBlockSplitRegex().Split(text.Replace("\r\n", "\n"))
            .Where(b => !string.IsNullOrWhiteSpace(b));

        foreach (var block in blocks)
        {
            var lines = block.Split('\n');
            var tIdx = -1;
            for (var i = 0; i < lines.Length; i++)
            {
                if (lines[i].Contains("-->"))
                {
                    tIdx = i;
                    break;
                }
            }
            if (tIdx < 0) continue;

            var tLine = lines[tIdx];
            var m = TimeArrowRegex().Match(tLine);
            if (!m.Success) continue;

            var start = ParseTime(m.Groups[1].Value);
            var end = ParseTime(m.Groups[2].Value);
            if (end <= start) continue;

            var textLines = lines[(tIdx + 1)..]
                .Where(l => !string.IsNullOrWhiteSpace(l));
            var raw = string.Join("<br>", textLines);
            var sanitized = SanitizeHtml(raw);

            subs.Add(new SubtitleLine { Start = start, End = end, Text = sanitized });
        }

        return subs;
    }

    public static List<SubtitleLine> ParseLrc(string text)
    {
        var subs = new List<SubtitleLine>();
        var lines = text.Replace("\r\n", "\n").Split('\n');

        foreach (var line in lines)
        {
            var m = LrcLineRegex().Match(line);
            if (!m.Success) continue;

            var min = int.Parse(m.Groups[1].Value);
            var sec = int.Parse(m.Groups[2].Value);
            var msStr = m.Groups[3].Success ? m.Groups[3].Value[1..] : "0";
            var ms = int.Parse(msStr.PadRight(3, '0')[..3]);
            var start = min * 60 + sec + ms / 1000.0;
            var txt = EscapeHtml(m.Groups[4].Value.Trim());
            if (string.IsNullOrEmpty(txt)) continue;

            subs.Add(new SubtitleLine { Start = start, End = 0, Text = txt });
        }

        subs.Sort((a, b) => a.Start.CompareTo(b.Start));

        for (var i = 0; i < subs.Count; i++)
        {
            subs[i].End = i < subs.Count - 1
                ? subs[i + 1].Start - 0.05
                : subs[i].Start + 5;
        }

        return subs;
    }

    public static List<SubtitleLine> ParseFile(string path)
    {
        var text = File.ReadAllText(path);
        var ext = Path.GetExtension(path).ToLowerInvariant();
        return ext switch
        {
            ".srt" => ParseSrt(text),
            ".lrc" => ParseLrc(text),
            _ => new List<SubtitleLine>()
        };
    }

    private static double ParseTime(string s)
    {
        s = s.Trim();
        double h = 0, min = 0, sec = 0, ms = 0;

        var m1 = HmsMsRegex().Match(s);
        var m2 = HmsRegex().Match(s);
        var m3 = MmMsRegex().Match(s);
        var m4 = MmRegex().Match(s);

        if (m1.Success)
        {
            h = int.Parse(m1.Groups[1].Value);
            min = int.Parse(m1.Groups[2].Value);
            sec = int.Parse(m1.Groups[3].Value);
            ms = int.Parse(m1.Groups[4].Value.PadRight(3, '0')[..3]);
        }
        else if (m2.Success)
        {
            h = int.Parse(m2.Groups[1].Value);
            min = int.Parse(m2.Groups[2].Value);
            sec = int.Parse(m2.Groups[3].Value);
        }
        else if (m3.Success)
        {
            min = int.Parse(m3.Groups[1].Value);
            sec = int.Parse(m3.Groups[2].Value);
            ms = int.Parse(m3.Groups[3].Value.PadRight(3, '0')[..3]);
        }
        else if (m4.Success)
        {
            min = int.Parse(m4.Groups[1].Value);
            sec = int.Parse(m4.Groups[2].Value);
        }

        return h * 3600 + min * 60 + sec + ms / 1000.0;
    }

    private static string SanitizeHtml(string html)
    {
        html = BrTagRegex().Replace(html, "\x00BR\x00");
        html = WhiteListTagRegex().Replace(html, "\x00$1$2\x00");
        html = AnyTagRegex().Replace(html, "");
        html = html.Replace("\x00BR\x00", "<br>");
        html = RestoreTagRegex().Replace(html, "<$1$2>");
        return html;
    }

    private static string EscapeHtml(string s)
    {
        return s.Replace("&", "&amp;")
                .Replace("<", "&lt;")
                .Replace(">", "&gt;");
    }

    [GeneratedRegex(@"\n\n+")]
    private static partial Regex SrtBlockSplitRegex();

    [GeneratedRegex(@"([\d:.]+)\s*-->\s*([\d:.]+)")]
    private static partial Regex TimeArrowRegex();

    [GeneratedRegex(@"^\[(\d+):(\d+)(\.\d+)?\](.*)")]
    private static partial Regex LrcLineRegex();

    [GeneratedRegex(@"^(\d+):(\d+):(\d+)[,.](\d+)$")]
    private static partial Regex HmsMsRegex();

    [GeneratedRegex(@"^(\d+):(\d+):(\d+)$")]
    private static partial Regex HmsRegex();

    [GeneratedRegex(@"^(\d+):(\d+)[,.](\d+)$")]
    private static partial Regex MmMsRegex();

    [GeneratedRegex(@"^(\d+):(\d+)$")]
    private static partial Regex MmRegex();

    [GeneratedRegex(@"<br\s*/?>", RegexOptions.IgnoreCase)]
    private static partial Regex BrTagRegex();

    [GeneratedRegex(@"<(\/?)(i|b|u)\b[^>]*>", RegexOptions.IgnoreCase)]
    private static partial Regex WhiteListTagRegex();

    [GeneratedRegex(@"<[^>]+>")]
    private static partial Regex AnyTagRegex();

    [GeneratedRegex(@"\x00(\/?)(i|b|u)\x00")]
    private static partial Regex RestoreTagRegex();
}
