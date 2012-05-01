@echo off
:: Copyright (c) 2012 The Chromium Authors. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: This batch file will try to sync the root directory.

setlocal

:: Will download svn and python.
:: If you don't want to install the depot_tools version of these tools, remove
:: the 'force' option on the next command. The tools will be installed only if
:: not already in the PATH environment variable.
call "%~dp0bootstrap\win\win_tools.bat" force
if errorlevel 1 goto :EOF
:: Now clear errorlevel so it can be set by other programs later.
set errorlevel=

:: Shall skip automatic update?
IF "%DEPOT_TOOLS_UPDATE%" == "0" GOTO :EOF

:: We need either .\.svn\. or .\.git\. to be able to sync.
IF EXIST "%~dp0.svn\." GOTO :SVN_UPDATE
IF EXIST "%~dp0.git\." GOTO :GIT_UPDATE
echo Error updating depot_tools, no revision tool found.
goto :EOF


:SVN_UPDATE
call svn up -q "%~dp0."
goto :EOF


:GIT_UPDATE
cd /d "%~dp0."
call git config remote.origin.fetch > NUL
if errorlevel 1 goto :GIT_SVN_UPDATE
call git fetch -q origin > NUL
call git rebase -q origin > NUL
goto :EOF

:GIT_SVN_UPDATE
cd /d "%~dp0."
call git svn rebase -q -q
goto :EOF
