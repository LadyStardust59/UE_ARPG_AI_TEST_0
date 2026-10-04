@echo off
chcp 65001 >nul
title 项目回退
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0rollback.ps1"
echo.
pause
