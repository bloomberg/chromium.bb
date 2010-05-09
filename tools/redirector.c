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
#include <windows.h>

#pragma comment(linker, "/entry:entry")

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
 *
 * Use the folliwong commands:
 *   cl /c /O2 /GS- redirector.c
 *   link /subsystem:console redirector.obj kernel32.lib
 */

void entry() {
  wchar_t selector[] = { L' ', L'-', 0, L'3', L'2', 0, 0, 0, 0, 0, 0, 0 };
  wchar_t *newpath = NULL, *oldpath = NULL, *cmdline, *delimeter;
  int nacl64, done;
  static STARTUPINFOW si = { sizeof(STARTUPINFOW), 0};
  static PROCESS_INFORMATION pi;

  nacl64 = 128; /* Nice default size, we'll increase it if needed */
  do {
    oldpath = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(nacl64));
    if (!oldpath)
      goto ShowErrorMessage;
    done = GetModuleFileNameW(NULL, oldpath, nacl64);
    if (done == 0)
      goto ShowErrorMessage;
    if (done >= nacl64-1) {
      nacl64 <<= 1;
      HeapFree(GetProcessHeap(), 0, oldpath);
      done = 0;
    }
  } while (!done);
  newpath = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(nacl64+64));
  if (!newpath)
    goto ShowErrorMessage;
  nacl64 = 0;
  delimeter = oldpath;
  while (*delimeter)
    ++delimeter;
  while (delimeter>oldpath && *delimeter != L'/' && *delimeter != L'\\')
    --delimeter;
  if (delimeter == oldpath)
    goto ShowErrorMessage;
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
  /* We don't need L'\0' here, but lstrcpynW will copy it anyway */
  lstrcpynW(newpath, oldpath, done+1);
  if (nacl64) {
    lstrcpyW(newpath + done, L"\\libexec\\nacl64-");
    cmdline = newpath + done + 9;
    done += 16; delimeter += 12;
    selector[3] = L'6';
    selector[4] = L'4';
  } else {
    lstrcpyW(newpath + done, L"\\libexec\\nacl-");
    cmdline = newpath + done + 9;
    done += 14; delimeter += 10;
  }
  lstrcpynW(newpath + done, delimeter, 32);
  if ((delimeter[0] == L'a' || delimeter[0] == L'A') &&
      (delimeter[1] == L's' || delimeter[0] == L'S') &&
      (delimeter[2] == '.'))
    selector[2] = '-';
  else if ((delimeter[0] == L'l' || delimeter[0] == L'L') &&
           (delimeter[1] == L'd' || delimeter[0] == L'D') &&
           (delimeter[2] == '.'))
    lstrcpyW(selector+2, L"melf_nacl");
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
    selector[2] = 'm',
    cmdline = newpath;
  HeapFree(GetProcessHeap(), 0, oldpath);
  oldpath = NULL;
  delimeter = GetCommandLineW();
  if (!delimeter)
    goto ShowErrorMessage;
  if (delimeter[0] == L'\"') {
    delimeter++;
    while (delimeter[0] && delimeter[0] != L'\"') {
      if (delimeter[0] == L'\\' &&
          (delimeter[1] == L'\"' || delimeter[1] == L'\\'))
        delimeter++;
      delimeter++;
    }
    delimeter++;
  } else {
    while (delimeter[0] && delimeter[0] != L' ')
      delimeter++;
  }
  nacl64 = lstrlenW(cmdline);
  if (selector[2])
    done = lstrlenW(selector);
  else
    done = 0;
  oldpath = HeapAlloc(GetProcessHeap(), 0,
                      (lstrlenW(delimeter)+nacl64+done+4)*sizeof(wchar_t));
  if (!oldpath)
    goto ShowErrorMessage;
  lstrcpyW(oldpath, cmdline);
  lstrcpyW(oldpath+nacl64, selector);
  lstrcpyW(oldpath+nacl64+done, delimeter);
  if (CreateProcessW(newpath, oldpath, NULL, NULL, TRUE,
                 CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi) != 0) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    HeapFree(GetProcessHeap(), 0, newpath);
    HeapFree(GetProcessHeap(), 0, oldpath);
    ExitProcess(0);
  }
 ShowErrorMessage:
  if (newpath)
    HeapFree(GetProcessHeap(), 0, newpath);
  if (oldpath)
    HeapFree(GetProcessHeap(), 0, oldpath);
#define ShowErrorMessage "Can not find filename to execute!\r\n"
  WriteFile(GetStdHandle(STD_ERROR_HANDLE),
            ShowErrorMessage, sizeof(ShowErrorMessage)-1, &nacl64, NULL);
}
