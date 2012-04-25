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

:: Try running the compiler. If it fails, then we assume we need to set up the
:: environment for the compiler.
:: TODO(scottmg): We should also try to detect if we have the right version
:: of the compiler too (i.e. If generation specified 2010, but we're running
:: from a 2008 command prompt).
cl 2>nul >nul
if not errorlevel 1 goto no_set_env

if not exist "%cd_path%\set_environment.bat" (
  echo ninja.bat: set_environment.bat not found in '%cd_path%'. Is -C arg correct?
  goto :EOF
)
call "%cd_path%\set_environment.bat"

:: Export only the path changes out of the script.
endlocal & set PATH=%PATH% & set INCLUDE=%INCLUDE% & set LIBPATH=%LIBPATH% & set LIB=%LIB%

:: To pair with below when we don't skip this block.
setlocal

:: Add python to the path, many gyp rules assume it's there.
:: Add ninja directory to the path (to find ninja and ninja-deplist-helper).
:: Put it at the front so that ninja.exe is found before this wrapper so that
:: next time we just run it directly (otherwise, this script adds 500-800ms to
:: ninja invocations).
:no_set_env
endlocal & set PATH=%~dp0python_bin;%~dp0ninja-win;%PATH%

:: Now run the actual build.
ninja.exe %*
