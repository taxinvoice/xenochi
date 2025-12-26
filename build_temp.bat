@echo off
echo Starting ESP-IDF build...
call C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat >nul 2>&1
cd /d D:\Data\git\xenochi
echo Running idf.py build...
idf.py build
echo Build completed with exit code %ERRORLEVEL%
