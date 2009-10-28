@echo off
setlocal
set PYTHONPATH=%~dp0..\..\third_party\pefile;%PYTHONPATH%
%~dp0..\..\third_party\python_24\python.exe %~dp0checkbin.py %*
