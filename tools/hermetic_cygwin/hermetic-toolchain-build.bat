: Copyright 2010, Google Inc.
: All rights reserved.
:
: Redistribution and use in source and binary forms, with or without
: modification, are permitted provided that the following conditions are
: met:
:
:     * Redistributions of source code must retain the above copyright
: notice, this list of conditions and the following disclaimer.
:     * Redistributions in binary form must reproduce the above
: copyright notice, this list of conditions and the following disclaimer
: in the documentation and/or other materials provided with the
: distribution.
:     * Neither the name of Google Inc. nor the names of its
: contributors may be used to endorse or promote products derived from
: this software without specific prior written permission.
:
: THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
: "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
: LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
: A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
: OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
: SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
: LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
: DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
: THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
: (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
: OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

@echo off
if exist "%~dp0..\..\cygwin" goto skip_cygwin
if exist "%~dp0hermetic_cygwin_1_7_5-1_0.exe" goto setup_downloaded
wget http://build.chromium.org/mirror/nacl/cygwin_mirror/hermetic_cygwin_1_7_5-1_0.exe -O "%~dp0hermetic_cygwin_1_7_5-1_0.exe"
if errorlevel 1 GoTo cyg_wget
chmod a+x "%~dp0\hermetic_cygwin_1_7_5-1_0.exe"
GoTo setup_downloaded
:cyg_wget
\cygwin\bin\wget http://build.chromium.org/mirror/nacl/cygwin_mirror/hermetic_cygwin_1_7_5-1_0.exe -O "%~dp0hermetic_cygwin_1_7_5-1_0.exe"
\cygwin\bin\chmod a+x "%~dp0hermetic_cygwin_1_7_5-1_0.exe"
:setup_downloaded
start /WAIT hermetic_cygwin_1_7_5-1_0.exe /DEVEL /S /D=%~dp0..\..\cygwin
:skip_cygwin
cd "%~dp0.."
set "PATH=%~dp0..\..\cygwin\bin;%PATH%"
bash -c 'MAKEINFO=`cygpath -u ${PWD}`/makeinfo_dummy make TOOLCHAINLOC="`cygpath -u ${PWD}`"/toolchain %* && tar cvfz "hermetic_cygwin/nacl-toolchain-win-x86-r$(svn info | grep Revision: | cut -b 11-)".tgz toolchain/'
