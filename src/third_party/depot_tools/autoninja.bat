@echo off
:: Copyright 2017 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

REM Set unique build ID.
FOR /f "usebackq tokens=*" %%a in (`python -c "from __future__ import print_function; import uuid; print(uuid.uuid4())"`) do set AUTONINJA_BUILD_ID=%%a

REM If a build performance summary has been requested then also set NINJA_STATUS
REM to trigger more verbose status updates. In particular this makes it possible
REM to see how quickly process creation is happening - often a critical clue on
REM Windows. The trailing space is intentional.
if "%NINJA_SUMMARIZE_BUILD%" == "1" set NINJA_STATUS=[%%r processes, %%f/%%t @ %%o/s : %%es ] 

REM Execute whatever is printed by autoninja.py.
REM Also print it to reassure that the right settings are being used.
FOR /f "usebackq tokens=*" %%a in (`vpython %~dp0autoninja.py "%*"`) do echo %%a & %%a
@if errorlevel 1 goto buildfailure

REM Use call to invoke vpython script here, because we use vpython via vpython.bat.
@if "%NINJA_SUMMARIZE_BUILD%" == "1" call vpython.bat %~dp0post_build_ninja_summary.py %*
@call vpython.bat %~dp0ninjalog_uploader_wrapper.py --cmdline %*

exit /b
:buildfailure

@call vpython.bat %~dp0ninjalog_uploader_wrapper.py --cmdline %*

REM Return an error code of 1 so that if a developer types:
REM "autoninja chrome && chrome" then chrome won't run if the build fails.
cmd /c exit 1
