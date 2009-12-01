@echo off
set TOOL_DIR=%~dp0
set NACL_ROOT=%TOOL_DIR%\..\..
set PRE_WINPY_PATH=%PATH%
set PATH=
set PYTHONPATH=
%NACL_ROOT%\third_party\\python_24\\setup_env.bat && python.exe %*

