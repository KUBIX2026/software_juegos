@echo off
rem Doble clic: compila los juegos, el menu y la app KUBIX.exe (necesita gcc y SDL2).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0compilar_windows.ps1" %*
