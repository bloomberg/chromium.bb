@echo off
:: Copyright (c) 2010 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

set TSAN_PATH=%~dp0..\..\third_party\tsan
set TSAN_SFX=%TSAN_PATH%\tsan-x86-windows-sfx.exe
if EXIST %TSAN_SFX% GOTO BINARY_OK
echo "Can't find ThreadSanitizer executables."
echo "See http://www.chromium.org/developers/how-tos/using-valgrind/threadsanitizer/threadsanitizer-on-windows"
echo "for the instructions on how to get them."
set %ERRORLEVEL% 1
goto :EOF

:BINARY_OK
%TSAN_SFX% -o%TSAN_PATH%\unpacked -y
set PIN_COMMAND=%TSAN_PATH%\unpacked\tsan-x86-windows\tsan.bat
set PYTHONPATH=%~dp0../python/google
python %~dp0/chrome_tests.py %*
