using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using RePlayer.Models;

namespace RePlayer.Services;

public static class FileService
{
    public static List<Track> LoadFromDirectory(string dirPath)
    {
        var tracks = new List<Track>();
        var mp3Files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var srtFiles = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var lrcFiles = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (var file in Directory.GetFiles(dirPath))
        {
            var ext = Path.GetExtension(file).ToLowerInvariant();
            var baseName = Path.GetFileNameWithoutExtension(file);

            switch (ext)
            {
                case ".mp3":
                    mp3Files[baseName] = file;
                    break;
                case ".srt":
                    srtFiles[baseName] = file;
                    break;
                case ".lrc":
                    lrcFiles[baseName] = file;
                    break;
            }
        }

        foreach (var (baseName, mp3Path) in mp3Files)
        {
            var subPath = srtFiles.TryGetValue(baseName, out var s)
                ? s
                : lrcFiles.TryGetValue(baseName, out var l)
                    ? l
                    : null;

            var track = new Track
            {
                Name = baseName,
                Mp3Path = mp3Path,
                SubtitlePath = subPath
            };

            if (subPath != null)
            {
                track.Subtitles = SubtitleParser.ParseFile(subPath);
            }

            tracks.Add(track);
        }

        tracks.Sort((a, b) => string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase));
        return tracks;
    }

    public static List<Track> LoadFromFiles(string[] filePaths)
    {
        var tracks = new List<Track>();
        var mp3Files = new List<(string Path, string BaseName)>();
        var srtFiles = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var lrcFiles = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (var file in filePaths)
        {
            var ext = Path.GetExtension(file).ToLowerInvariant();
            var baseName = Path.GetFileNameWithoutExtension(file);

            switch (ext)
            {
                case ".mp3":
                    mp3Files.Add((file, baseName));
                    break;
                case ".srt":
                    srtFiles[baseName] = file;
                    break;
                case ".lrc":
                    lrcFiles[baseName] = file;
                    break;
            }
        }

        foreach (var (mp3Path, baseName) in mp3Files)
        {
            var subPath = srtFiles.TryGetValue(baseName, out var s)
                ? s
                : lrcFiles.TryGetValue(baseName, out var l)
                    ? l
                    : null;

            var track = new Track
            {
                Name = baseName,
                Mp3Path = mp3Path,
                SubtitlePath = subPath
            };

            if (subPath != null)
            {
                track.Subtitles = SubtitleParser.ParseFile(subPath);
            }

            tracks.Add(track);
        }

        return tracks;
    }
}
