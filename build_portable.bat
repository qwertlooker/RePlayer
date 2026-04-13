@echo off
setlocal

if not exist .venv (
  py -3.11 -m venv .venv
)

call .venv\Scripts\activate.bat
python -m pip install --upgrade pip
pip install -r requirements.txt pyinstaller

REM python-vlc needs VLC runtime dlls. Put libvlc.dll and plugins folder next to exe.
pyinstaller --noconfirm --onefile --windowed --name RePlayer main.py

echo Build complete. See dist\RePlayer.exe
