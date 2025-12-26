@echo off
call C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat
cd /d D:\Data\git\xenochi
idf.py build
