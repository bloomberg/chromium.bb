@echo off

:: Copyright (c) 2012 Google Inc. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal enabledelayedexpansion

:: Parse args to see if a -C argument (to change working directory) is being
:: supplied. We expect project generation to output a set_environment.bat that
:: will set up the environment (variables and path). This script generally
:: just calls the correct VS vars batch file, but only gyp has the knowledge
:: to determine which version of the IDE/toolchain it wants to use, so we have
:: to defer to it to make that decision.
set found_dash_c=0
set cd_path=.
for %%A in (%*) do (
  if "!found_dash_c!"=="1" (
    set cd_path=%%A
    goto done_dash_c
  )
  if "%%A"=="-C" (
    set found_dash_c=1
  )
)
:done_dash_c

if not exist "%cd_path%\set_environment.bat" (
  echo ninja.bat: set_environment.bat not found in '%cd_path%'. Is -C arg correct?
  goto :EOF
)
call "%cd_path%\set_environment.bat"

:: Add python to the path, many gyp rules assume it's there.
:: Add ninja directory to the path (to find ninja and ninja-deplist-helper).
:: Then, export only the path changes out of the script so that next time we
:: just run ninja directly (otherwise, this script adds about 500-800ms to
:: ninja invocations).
endlocal & set PATH=%~dp0python_bin;%~dp0ninja-win;%PATH% & set INCLUDE=%INCLUDE% & set LIBPATH=%LIBPATH% & set LIB=%LIB%

:: Now run the actual build.
ninja.exe %*
