@echo off
set TOOL_DIR=%~dp0
set NACL_ROOT=%TOOL_DIR%\..\..
set PRE_WINPY_PATH=%PATH%
set PATH=%NACL_ROOT%\third_party\python_26
set PYTHONPATH=
python.exe %*
