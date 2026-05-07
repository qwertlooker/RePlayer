using System;
using NAudio.Wave;

namespace RePlayer.Services;

public class AudioPlayer : IDisposable
{
    private WaveOutEvent? _waveOut;
    private AudioFileReader? _reader;
    private bool _disposed;

    public event EventHandler? MediaOpened;
    public event EventHandler? MediaEnded;

    public TimeSpan Position
    {
        get => _reader?.CurrentTime ?? TimeSpan.Zero;
        set { if (_reader != null) _reader.CurrentTime = value; }
    }

    public TimeSpan Duration => _reader?.TotalTime ?? TimeSpan.Zero;

    public double Volume
    {
        get => _reader?.Volume ?? 1.0;
        set { if (_reader != null) _reader.Volume = (float)Math.Clamp(value, 0, 1); }
    }

    public bool IsMuted { get; set; }

    public bool IsPaused => _waveOut?.PlaybackState != PlaybackState.Playing;

    public void Open(string path)
    {
        Close();
        try
        {
            _reader = new AudioFileReader(path);
            _waveOut = new WaveOutEvent();
            _waveOut.Init(_reader);
            _waveOut.PlaybackStopped += OnPlaybackStopped;
            MediaOpened?.Invoke(this, EventArgs.Empty);
        }
        catch
        {
            Close();
        }
    }

    public void Play()
    {
        _waveOut?.Play();
    }

    public void Pause()
    {
        _waveOut?.Pause();
    }

    public void Close()
    {
        if (_waveOut != null)
        {
            _waveOut.PlaybackStopped -= OnPlaybackStopped;
            _waveOut.Stop();
            _waveOut.Dispose();
            _waveOut = null;
        }
        if (_reader != null)
        {
            _reader.Dispose();
            _reader = null;
        }
    }

    private void OnPlaybackStopped(object? sender, StoppedEventArgs e)
    {
        if (_reader != null && _reader.CurrentTime >= _reader.TotalTime - TimeSpan.FromMilliseconds(500))
        {
            MediaEnded?.Invoke(this, EventArgs.Empty);
        }
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            Close();
            _disposed = true;
        }
        GC.SuppressFinalize(this);
    }
}
