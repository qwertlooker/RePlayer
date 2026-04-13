# RePlayer (Windows 免安装英语学习播放器)

## A. 技术方案
- **语言与 UI**：Python 3.11 + Tkinter（标准库，稳定、简单，适合 v1）。
- **音频播放**：`python-vlc` 调用 VLC 内核播放本地 MP3，支持毫秒级 `seek`，满足字幕点击跳转与逐句定位。
- **字幕系统**：纯本地解析 `.srt/.lrc`，统一转成 `SubtitleLine(start_ms,end_ms,text)`。
- **播放状态机**：80ms 轮询当前播放时间，完成：
  - 当前句高亮
  - 单句复读
  - 句间自动暂停（可配置 ms）
- **录音系统**：`sounddevice` 录制麦克风为 WAV（16k/16bit/mono），按句编号存储。
- **对比播放**：
  - 原音：按当前句 `start_ms/end_ms` 播放片段
  - 录音：直接播放对应 WAV
- **离线与便携**：全部本地运行；使用 `PyInstaller --onefile --windowed` 打包成单 exe（无需安装/管理员）。

## B. 项目目录结构
```text
RePlayer/
├─ main.py
├─ requirements.txt
├─ build_portable.bat
├─ README.md
├─ .github/workflows/release.yml   # GitHub Action 自动构建并发布 Release
└─ src/replayer/
   ├─ __init__.py
   ├─ app.py
   ├─ audio.py
   ├─ recording.py
   ├─ subtitles.py
   └─ models.py
```

## C. 依赖说明
- `python-vlc`：MP3 播放、seek、时间获取。
- `sounddevice`：麦克风采集录音。
- `simpleaudio`：播放录音 WAV 文件。
- `tkinter`：Python 标准库 GUI（无需额外安装）。

## D. 分阶段实现计划
1. **Phase 1（已实现）基础可用**
   - MP3 打开/播放
   - SRT/LRC 加载显示
   - 点击字幕跳转 + 当前句高亮
2. **Phase 2（已实现）学习核心**
   - 单句复读
   - 句间自动暂停（可配）
3. **Phase 3（已实现）跟读对比**
   - 当前句录音
   - 原音片段与录音分别播放
4. **Phase 4（已实现）便携发布**
   - 本地 `build_portable.bat` 打包
   - GitHub Action 自动构建并发布 Release（含 exe）

## 本地运行
```bash
python -m venv .venv
source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python main.py
```

## 本地 Windows 打包
```bat
build_portable.bat
```
产物：`dist\RePlayer.exe`

## GitHub Action 自动发布（推荐）
1. 推送代码到主干（`main`）。
2. 打一个版本标签并推送：
   ```bash
   git tag v0.1.0
   git push origin v0.1.0
   ```
3. Action `Build and Release Windows Portable` 会：
   - 在 `windows-latest` 安装 Python 与 VLC runtime
   - PyInstaller 打包 `RePlayer.exe`
   - 自动创建 GitHub Release 并上传 `RePlayer-vX.Y.Z-windows-x64.zip`

## 当前 v1 已覆盖的需求
- [x] 打开本地 MP3
- [x] 加载并显示 SRT / LRC
- [x] 字幕列表点击跳转
- [x] 当前播放句高亮
- [x] 单句复读
- [x] 句间自动暂停（可配置）
- [x] 当前句跟读录音
- [x] 分别播放原音与录音
- [x] Windows 免安装（本地/CI 打包）
