@echo off
cd /d "%~dp0"
python -X utf8 tools\launch_studio.py
if errorlevel 1 pause
