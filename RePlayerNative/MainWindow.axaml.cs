using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using RePlayer.Models;
using RePlayer.Services;

namespace RePlayer;

public partial class MainWindow : Window
{
    private readonly List<Models.Track> _tracks = new();
    private int _curTrack = -1;
    private List<SubtitleLine> _subs = new();
    private int _activeSub = -1;

    private bool _repeatOn;
    private int _repeatIndex = -1;
    private double _repeatStart;
    private double _repeatEnd;

    private double _lastVol = 1;
    private bool _seeking;

    private static readonly double[] Speeds = { 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0 };
    private int _speedIdx = 5;

    private readonly DispatcherTimer _timer;
    private readonly AudioPlayer _player = new();

    public MainWindow()
    {
        InitializeComponent();

        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        _timer.Tick += Timer_Tick;

        _player.MediaOpened += Player_MediaOpened;
        _player.MediaEnded += Player_MediaEnded;

        BtnOpenDir.Click += BtnOpenDir_Click;
        BtnOpenFiles.Click += BtnOpenFiles_Click;

        UpdatePlayBtn();
        UpdateVolIcon();
    }

    private void Player_MediaOpened(object? sender, EventArgs e)
    {
        Dispatcher.UIThread.Post(() =>
        {
            ProgressSlider.Maximum = _player.Duration.TotalSeconds;
            TimeDur.Text = FormatTime(ProgressSlider.Maximum);
        });
    }

    private void Player_MediaEnded(object? sender, EventArgs e)
    {
        Dispatcher.UIThread.Post(UpdatePlayBtn);
    }

    private void Timer_Tick(object? sender, EventArgs e)
    {
        if (_seeking) return;
        var ct = _player.Position.TotalSeconds;

        if (_repeatOn && _repeatIndex >= 0)
        {
            if (ct >= _repeatEnd)
            {
                _player.Position = TimeSpan.FromSeconds(_repeatStart);
                return;
            }
        }

        UpdateProgress(ct);
        SyncSubtitle(ct);
    }

    private void UpdateProgress(double ct)
    {
        if (ProgressSlider.Maximum <= 0) return;
        _seeking = true;
        ProgressSlider.Value = ct;
        _seeking = false;
        TimeCur.Text = FormatTime(ct);
        TimeDur.Text = FormatTime(_player.Duration.TotalSeconds);
    }

    private void SyncSubtitle(double ct)
    {
        var found = -1;
        for (var i = 0; i < _subs.Count; i++)
        {
            if (ct >= _subs[i].Start && ct < _subs[i].End)
            {
                found = i;
                break;
            }
        }

        if (found != _activeSub)
        {
            _activeSub = found;
            UpdateSubHighlight();
            UpdateCurrentSub();
        }
    }

    private void UpdateSubHighlight()
    {
        if (_activeSub >= 0 && _activeSub < SubList.ItemCount)
        {
            SubList.SelectedIndex = _activeSub;
            SubList.ScrollIntoView(_activeSub);
        }
        else
        {
            SubList.SelectedIndex = -1;
        }
    }

    private void UpdateCurrentSub()
    {
        if (_activeSub >= 0 && _activeSub < _subs.Count)
        {
            CurrentSub.Text = StripHtmlTags(_subs[_activeSub].Text);
        }
        else
        {
            CurrentSub.Text = "";
        }
    }

    private void ClickSub(int i)
    {
        if (i < 0 || i >= _subs.Count) return;
        if (_repeatOn)
        {
            EngageRepeat(i);
        }
        else
        {
            _player.Position = TimeSpan.FromSeconds(_subs[i].Start);
            if (_player.IsPaused) Play();
        }
    }

    private void EngageRepeat(int i)
    {
        _repeatIndex = i;
        _repeatStart = _subs[i].Start;
        _repeatEnd = _subs[i].End;
        _player.Position = TimeSpan.FromSeconds(_repeatStart);
        if (_player.IsPaused) Play();
        UpdateRepeatUI();
    }

    private void ToggleRepeat()
    {
        if (_repeatOn)
        {
            _repeatOn = false;
            _repeatIndex = -1;
            _repeatStart = 0;
            _repeatEnd = 0;
        }
        else
        {
            _repeatOn = true;
            if (_activeSub >= 0) EngageRepeat(_activeSub);
        }

        UpdateRepeatUI();
    }

    private void UpdateRepeatUI()
    {
        if (_repeatOn)
            BtnRepeat.Classes.Add("repeat-on");
        else
            BtnRepeat.Classes.Remove("repeat-on");
    }

    private void PrevSub()
    {
        var target = Math.Max(0, _activeSub - 1);
        ClickSub(target);
    }

    private void NextSub()
    {
        var target = Math.Min(_subs.Count - 1, _activeSub + 1);
        ClickSub(target);
    }

    private void Play()
    {
        _player.Play();
        _timer.Start();
        UpdatePlayBtn();
    }

    private void Pause()
    {
        _player.Pause();
        UpdatePlayBtn();
    }

    private void TogglePlay()
    {
        if (_curTrack < 0) return;
        if (_player.IsPaused) Play();
        else Pause();
    }

    private void UpdatePlayBtn()
    {
        BtnPlay.Content = _player.IsPaused ? "▶" : "⏸";
    }

    private void SetSpeed(int idx)
    {
        _speedIdx = Math.Clamp(idx, 0, Speeds.Length - 1);
        SpeedLabel.Text = $"{Speeds[_speedIdx]:0.###}×";
    }

    private void UpdateVolIcon()
    {
        var v = _player.Volume;
        VolIcon.Content = v == 0 || _player.IsMuted ? "🔇" : v < 0.5 ? "🔉" : "🔊";
    }

    private void ToggleMute()
    {
        if (_player.IsMuted || _player.Volume == 0)
        {
            _player.IsMuted = false;
            _player.Volume = _lastVol > 0 ? _lastVol : 1;
            VolSlider.Value = _player.Volume;
        }
        else
        {
            _lastVol = _player.Volume;
            _player.IsMuted = true;
            VolSlider.Value = 0;
        }
        UpdateVolIcon();
    }

    private void LoadTrack(int idx)
    {
        if (idx < 0 || idx >= _tracks.Count) return;
        _curTrack = idx;
        var t = _tracks[idx];

        _player.Open(t.Mp3Path);

        _activeSub = -1;
        _repeatOn = false;
        _repeatIndex = -1;
        _repeatStart = 0;
        _repeatEnd = 0;
        UpdateRepeatUI();

        _subs = t.Subtitles.Count > 0 ? t.Subtitles :
                t.SubtitlePath != null ? SubtitleParser.ParseFile(t.SubtitlePath) : new List<SubtitleLine>();
        t.Subtitles = _subs;

        TrackName.Text = t.Name;
        RenderTrackList();
        RenderSubList();
        UpdateCurrentSub();
        UpdatePlayBtn();

        Play();
    }

    private void RenderTrackList()
    {
        TrackList.ItemsSource = _tracks.Select(t => t.Name).ToList();
        if (_curTrack >= 0 && _curTrack < _tracks.Count)
            TrackList.SelectedIndex = _curTrack;
    }

    private void RenderSubList()
    {
        SubList.ItemsSource = _subs.Select(s => $"[{s.StartTimeFormatted}] {StripHtmlTags(s.Text)}").ToList();
    }

    private void AddTrack(string name, string mp3Path, string? subPath)
    {
        var existing = _tracks.FirstOrDefault(t =>
            string.Equals(t.Name, name, StringComparison.OrdinalIgnoreCase));
        if (existing != null)
        {
            if (subPath != null && existing.SubtitlePath == null)
            {
                existing.SubtitlePath = subPath;
                existing.Subtitles = SubtitleParser.ParseFile(subPath);
            }
            return;
        }

        var track = new Models.Track
        {
            Name = name,
            Mp3Path = mp3Path,
            SubtitlePath = subPath
        };
        if (subPath != null)
            track.Subtitles = SubtitleParser.ParseFile(subPath);

        _tracks.Add(track);
    }

    private async Task ProcessFiles(string[] filePaths)
    {
        var mp3s = new List<(string Path, string Base)>();
        var srts = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var lrcs = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (var f in filePaths)
        {
            var ext = Path.GetExtension(f).ToLowerInvariant();
            var baseName = Path.GetFileNameWithoutExtension(f);
            switch (ext)
            {
                case ".mp3": mp3s.Add((f, baseName)); break;
                case ".srt": srts[baseName] = f; break;
                case ".lrc": lrcs[baseName] = f; break;
            }
        }

        foreach (var (mp3Path, baseName) in mp3s)
        {
            var subPath = srts.TryGetValue(baseName, out var s) ? s :
                          lrcs.TryGetValue(baseName, out var l) ? l : null;
            AddTrack(baseName, mp3Path, subPath);
        }

        if (mp3s.Count == 0)
        {
            foreach (var f in filePaths)
            {
                var ext = Path.GetExtension(f).ToLowerInvariant();
                if ((ext == ".srt" || ext == ".lrc") && _curTrack >= 0)
                {
                    var t = _tracks[_curTrack];
                    if (t.SubtitlePath == null)
                    {
                        t.SubtitlePath = f;
                        t.Subtitles = SubtitleParser.ParseFile(f);
                        _subs = t.Subtitles;
                        RenderSubList();
                    }
                }
            }
        }

        RenderTrackList();

        if (_curTrack < 0 && _tracks.Count > 0)
            LoadTrack(0);
    }

    private async void BtnOpenDir_Click(object? sender, RoutedEventArgs e)
    {
        var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "选择音频文件夹",
            AllowMultiple = false
        });

        if (folders.Count == 0) return;
        var folder = folders[0];
        var dirPath = folder.TryGetLocalPath();
        if (dirPath == null) return;

        var tracks = FileService.LoadFromDirectory(dirPath);
        foreach (var t in tracks)
            AddTrack(t.Name, t.Mp3Path, t.SubtitlePath);

        RenderTrackList();
        if (_curTrack < 0 && _tracks.Count > 0)
            LoadTrack(0);
    }

    private async void BtnOpenFiles_Click(object? sender, RoutedEventArgs e)
    {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = "选择音频或字幕文件",
            AllowMultiple = true,
            FileTypeFilter = new List<FilePickerFileType>
            {
                new("音频与字幕")
                {
                    Patterns = new[] { "*.mp3", "*.srt", "*.lrc" }
                }
            }
        });

        if (files.Count == 0) return;
        var paths = files.Select(f => f.TryGetLocalPath()).Where(p => p != null).Cast<string>().ToArray();
        if (paths.Length > 0)
            await ProcessFiles(paths);
    }

    private void TrackList_SelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (TrackList.SelectedIndex >= 0 && TrackList.SelectedIndex != _curTrack)
        {
            LoadTrack(TrackList.SelectedIndex);
        }
    }

    private void SubList_SelectionChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (SubList.SelectedIndex >= 0 && SubList.SelectedIndex != _activeSub)
        {
            ClickSub(SubList.SelectedIndex);
        }
    }

    private void BtnPrev_Click(object? sender, RoutedEventArgs e) => PrevSub();
    private void BtnPlay_Click(object? sender, RoutedEventArgs e) => TogglePlay();
    private void BtnNext_Click(object? sender, RoutedEventArgs e) => NextSub();
    private void BtnRepeat_Click(object? sender, RoutedEventArgs e) => ToggleRepeat();

    private void BtnSpeedDown_Click(object? sender, RoutedEventArgs e) => SetSpeed(_speedIdx - 1);
    private void BtnSpeedUp_Click(object? sender, RoutedEventArgs e) => SetSpeed(_speedIdx + 1);

    private void VolIcon_Click(object? sender, RoutedEventArgs e) => ToggleMute();

    private void VolSlider_ValueChanged(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (_player == null) return;
        _player.Volume = e.NewValue;
        if (e.NewValue > 0)
        {
            _player.IsMuted = false;
            _lastVol = e.NewValue;
        }
        UpdateVolIcon();
    }

    private void ProgressSlider_ValueChanged(object? sender, RangeBaseValueChangedEventArgs e)
    {
        if (_seeking || _player == null) return;
        _player.Position = TimeSpan.FromSeconds(e.NewValue);
    }

    private void Progress_PointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (sender is not Border border) return;
        var pos = e.GetPosition(border);
        var pct = pos.X / border.Bounds.Width;
        if (_player.Duration.TotalSeconds > 0)
        {
            var target = pct * _player.Duration.TotalSeconds;
            _player.Position = TimeSpan.FromSeconds(target);
        }
    }

    private void Window_KeyDown(object? sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Space:
                e.Handled = true;
                TogglePlay();
                break;
            case Key.Up:
            case Key.Left:
                e.Handled = true;
                PrevSub();
                break;
            case Key.Down:
            case Key.Right:
                e.Handled = true;
                NextSub();
                break;
            case Key.R:
                ToggleRepeat();
                break;
            case Key.M:
                ToggleMute();
                break;
            case Key.OemComma:
                SetSpeed(_speedIdx - 1);
                break;
            case Key.OemPeriod:
                SetSpeed(_speedIdx + 1);
                break;
        }
    }

    private static string FormatTime(double seconds)
    {
        seconds = Math.Max(0, seconds);
        var m = (int)(seconds / 60);
        var s = (int)(seconds % 60);
        return $"{m:D2}:{s:D2}";
    }

    private static string StripHtmlTags(string html)
    {
        var result = html.Replace("<br>", " ").Replace("<br/>", " ").Replace("<br />", " ");
        var inTag = false;
        var output = new System.Text.StringBuilder();
        foreach (var c in result)
        {
            if (c == '<') { inTag = true; continue; }
            if (c == '>') { inTag = false; continue; }
            if (!inTag) output.Append(c);
        }
        return output.ToString();
    }

    protected override void OnClosing(WindowClosingEventArgs e)
    {
        _player.Dispose();
        base.OnClosing(e);
    }
}
