# RePlayer

RePlayer 是一个轻量、免安装、面向英语学习的 Windows 本地播放器。当前版本基于 Win32 + C++17 + CMake，覆盖 v1 需要的基础播放、字幕同步、单句复读、自动暂停、句级录音、原音与录音对比，以及绿色版打包流程。

## 构建

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Release 打包：

```powershell
cmake -S . -B build-release -G "Visual Studio 17 2022" -A x64 -DREPLAYER_ENABLE_STATIC_CRT=OFF
cmake --build build-release --config Release
cmake --install build-release --config Release --prefix out
powershell -ExecutionPolicy Bypass -File .\scripts\verify_package.ps1 -PackageRoot .\out
```

## 功能

- 打开本地 MP3/WAV
- 播放、暂停、停止、拖动进度
- 加载 SRT/LRC 字幕并跟随高亮
- 点击字幕跳转到对应句子
- `RepeatOne` 单句复读
- `AutoPause` 句间自动暂停与延时继续
- 当前句录音，输出 `recordings/<sentence-id>.wav`
- 播放当前句原音片段与当前句录音
- 日志输出到 `logs/app.log`

## 验收清单

- 阶段 2：可打开本地音频并完成播放、暂停、停止、seek
- 阶段 3：字幕可解析、同步高亮、点击跳转
- 阶段 4：复读和自动暂停可稳定触发，且 `RepeatOne` 优先
- 阶段 5：句级录音生成有效 WAV 文件
- 阶段 6：原音/录音对比按钮可重复触发
- 阶段 7：无字幕、拖动进度、播放结束等边界保持稳定
- 阶段 8：`out/` 为单目录绿色版，可直接复制运行

## 干净系统验证

1. 将 `out/` 整体复制到新目录。
2. 运行 [scripts/verify_package.ps1](/D:/LLM/project/Replayer/RePlayer/scripts/verify_package.ps1)。
3. 双击 `RePlayer.exe`，确认窗口可打开。
4. 打开本地音频和字幕，完成播放、字幕跳转、录音、原音/录音对比。
5. 检查 `logs/app.log` 和 `recordings/` 是否正常生成。
