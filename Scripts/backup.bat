@echo off
chcp 65001 >nul
title 项目备份
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0backup.ps1"
echo.
pause
