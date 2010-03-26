/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <wchar.h>
#include <process.h>
#include <windows.h>

#undef MAX_PATH
#define MAX_PATH 2048
#define ERROR { wprintf(L"Can not find filename to execute!"); return 1; }

/*
 * The SDK redirector reads it's name and invokes the appropriate binary from a
 * location with cygwin1.dll nearby so that all cygwin-oriented SDK programs can
 * start (this works for Cygwin version >= 1.7).
 *
 * Supports arbitrary unicode names in it's base directory name.
 *
 * The executable must be placed in nacl/bin or nacl64/bin or just bin (in the
 * latter case the name of the binary should be prefixed with nacl- or nacl64-)
 *
 * The binary should be built by visual studio (not cygwin) since when it starts
 * cygwin has not been located yet.
 */

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
  wchar_t newpath[MAX_PATH+64];
  wchar_t oldpath[MAX_PATH];
  wchar_t selector[] = { L'-', 0, L'3', L'2', 0, 0, 0, 0, 0, 0, 0 };
  wchar_t *delimeter = oldpath;
  int nacl64 = 0, done;

  if (GetModuleFileNameW(NULL, oldpath, MAX_PATH) == 0)
    ERROR;
  while (*delimeter)
    ++delimeter;
  if (delimeter-oldpath>MAX_PATH)
    ERROR;
  while (delimeter>oldpath && *delimeter != L'/' && *delimeter != L'\\')
    --delimeter;
  if (delimeter == oldpath)
    ERROR;

  if ((delimeter[-4] == L'/' || delimeter[-4] == L'\\') &&
      (delimeter[-3] == L'b' || delimeter[-3] == L'B') &&
      (delimeter[-2] == L'i' || delimeter[-2] == L'I') &&
      (delimeter[-1] == L'n' || delimeter[-1] == L'N') &&
      (delimeter[1] == L'n' || delimeter[1] == L'N') &&
      (delimeter[2] == L'a' || delimeter[2] == L'A') &&
      (delimeter[3] == L'c' || delimeter[3] == L'C') &&
      (delimeter[4] == L'l' || delimeter[4] == L'L') &&
       delimeter[5] == L'-') {
    delimeter-=4;
  } else if ((delimeter[-4] == L'/' || delimeter[-4] == L'\\') &&
           (delimeter[-3] == L'b' || delimeter[-3] == L'B') &&
           (delimeter[-2] == L'i' || delimeter[-2] == L'I') &&
           (delimeter[-1] == L'n' || delimeter[-1] == L'N') &&
           (delimeter[1] == L'n' || delimeter[1] == L'N') &&
           (delimeter[2] == L'a' || delimeter[2] == L'A') &&
           (delimeter[3] == L'c' || delimeter[3] == L'C') &&
           (delimeter[4] == L'l' || delimeter[4] == L'L') &&
           (delimeter[5] == L'6' || delimeter[5] == L'6') &&
           (delimeter[6] == L'4' || delimeter[6] == L'4') &&
            delimeter[7] == L'-') {
    delimeter-=4;
    nacl64=1;
  } else if ((delimeter[-9] == L'/' || delimeter[-9] == L'\\') &&
             (delimeter[-8] == L'n' || delimeter[-8] == L'N') &&
             (delimeter[-7] == L'a' || delimeter[-7] == L'A') &&
             (delimeter[-6] == L'c' || delimeter[-6] == L'C') &&
             (delimeter[-5] == L'l' || delimeter[-5] == L'L') &&
             (delimeter[-4] == L'/' || delimeter[-4] == L'\\') &&
             (delimeter[-3] == L'b' || delimeter[-3] == L'B') &&
             (delimeter[-2] == L'i' || delimeter[-2] == L'I') &&
             (delimeter[-1] == L'n' || delimeter[-1] == L'N')) {
    delimeter-=9;
  } else if ((delimeter[-11] == L'/' || delimeter[-11] == L'\\') &&
             (delimeter[-10] == L'n' || delimeter[-10] == L'N') &&
             (delimeter[-9] == L'a' || delimeter[-9] == L'A') &&
             (delimeter[-8] == L'c' || delimeter[-8] == L'C') &&
             (delimeter[-7] == L'l' || delimeter[-7] == L'L') &&
             (delimeter[-6] == L'6' || delimeter[-6] == L'6') &&
             (delimeter[-5] == L'4' || delimeter[-5] == L'4') &&
             (delimeter[-4] == L'/' || delimeter[-4] == L'\\') &&
             (delimeter[-3] == L'b' || delimeter[-3] == L'B') &&
             (delimeter[-2] == L'i' || delimeter[-2] == L'I') &&
             (delimeter[-1] == L'n' || delimeter[-1] == L'N')) {
    delimeter-=11;
    nacl64=1;
  }
  done = delimeter-oldpath;
  wmemcpy(newpath, oldpath, done);
  if (nacl64) {
    wmemcpy(newpath + done, L"\\libexec\\nacl64-", 16);
    done += 16; delimeter += 12;
    selector[2] = L'6';
    selector[3] = L'4';
  } else {
    wmemcpy(newpath + done, L"\\libexec\\nacl-", 14);
    done += 14; delimeter += 10;
  }
  wcscpy(newpath + done, delimeter);
  if ((delimeter[0] == L'a' || delimeter[0] == L'A') &&
      (delimeter[1] == L's' || delimeter[0] == L'S') &&
      (delimeter[2] == '.'))
    selector[1] = '-';
  else if ((delimeter[0] == L'l' || delimeter[0] == L'L') &&
           (delimeter[1] == L'd' || delimeter[0] == L'D') &&
           (delimeter[2] == '.'))
    wcsncpy(selector+1, L"melf_nacl", 9);
  else if (((delimeter[0] == L'c' || delimeter[0] == L'C') &&
            (delimeter[1] == L'+') && (delimeter[2] == L'+') &&
            (delimeter[3] == '.')) ||
           ((delimeter[0] == L'c' || delimeter[0] == L'C') &&
            (delimeter[1] == L'p' || delimeter[1] == L'P') &&
            (delimeter[2] == L'p' || delimeter[2] == L'P') &&
            (delimeter[3] == '.')) ||
           ((delimeter[0] == L'g' || delimeter[0] == L'G') &&
            (delimeter[1] == L'+') && (delimeter[2] == L'+') &&
            (delimeter[3] == '.')) ||
           ((delimeter[0] == L'g' || delimeter[0] == L'G') &&
            (delimeter[1] == L'c' || delimeter[1] == L'C') &&
            (delimeter[2] == L'c' || delimeter[2] == L'C') &&
            (delimeter[3] == '.') || (delimeter[3] == '-')) ||
           ((delimeter[0] == L'g' || delimeter[0] == L'G') &&
            (delimeter[1] == L'f' || delimeter[1] == L'F') &&
            (delimeter[2] == L'o' || delimeter[2] == L'O') &&
            (delimeter[3] == L'r' || delimeter[3] == L'R') &&
            (delimeter[4] == L't' || delimeter[4] == L'T') &&
            (delimeter[5] == L'r' || delimeter[5] == L'R') &&
            (delimeter[6] == L'a' || delimeter[6] == L'A') &&
            (delimeter[7] == L'n' || delimeter[7] == L'N') &&
            (delimeter[8] == '.')))
    selector[1] = 'm';
  if (selector[1]) {
    memcpy(((wchar_t**)oldpath)+2, argv+1, sizeof(wchar_t*)*(argc-1));
    argv = (wchar_t**)oldpath;
    argv[1] = selector;
    argv[argc+1] = 0;
  } else {
    memcpy(((wchar_t**)oldpath)+1, argv+1, sizeof(wchar_t*)*(argc-1));
    argv = (wchar_t**)oldpath;
    argv[argc] = 0;
  }
  argv[0] = newpath;
  return _wspawnvpe(_P_WAIT, newpath, argv, envp);
}
