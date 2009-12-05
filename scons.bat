@echo off

setlocal

:: If this batch file is run directly thru cygwin, we will get the wrong
:: version of python. To avoid this, if we detect cygwin, we need to then
:: invoke the shell script which will then re-invoke this batch file with
:: cygwin stripped out of the path.
:: Detect cygwin by trying to run bash.
bash --version >NUL 2>&1
if %ERRORLEVEL% == 0 (
    bash "%~dp0\scons" %* || exit 1
    goto end
)

:: Preserve a copy of the PATH (in case we need it later, mainly for cygwin).
set PRESCONS_PATH=%PATH%

:: Set the PYTHONPATH so we can import SCons modules
set PYTHONPATH=%~dp0..\third_party\scons\scons-local;%PYTHONPATH%

:: Add gnu_binutils to the path
set PATH=%PATH%;%~dp0src\third_party\gnu_binutils\files

:: Run the included copy of scons.
python -O -OO "%~dp0\..\third_party\scons\scons.py" %*

:end
