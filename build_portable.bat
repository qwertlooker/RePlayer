@echo off
setlocal

if not exist .venv (
  py -3.11 -m venv .venv
)

call .venv\Scripts\activate.bat
python -m pip install --upgrade pip
pip install -r requirements.txt pyinstaller

set VLC_PATH=%ProgramFiles%\VideoLAN\VLC
if not exist "%VLC_PATH%\libvlc.dll" (
  echo [ERROR] VLC runtime not found: %VLC_PATH%
  echo Please install VLC desktop first, then rerun this script.
  exit /b 1
)

pyinstaller --noconfirm --onefile --windowed --name RePlayer ^
  --add-binary "%VLC_PATH%\libvlc.dll;." ^
  --add-binary "%VLC_PATH%\libvlccore.dll;." ^
  --add-data "%VLC_PATH%\plugins;plugins" ^
  main.py

echo Build complete. See dist\RePlayer.exe
