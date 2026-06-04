@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-renode-capture.ps1" %*
