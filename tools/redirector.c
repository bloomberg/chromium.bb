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

#define FILE_NOT_FOUND_ERROR_MESSAGE "Can not find filename to execute!\r\n"

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
 *   link /subsystem:console /MERGE:.rdata=.text /NODEFAULTLIB redirector.obj kernel32.lib
 */

__inline void get_module_name_safely_and_allocate_buffer_for_rewrite(
  wchar_t **oldpath, wchar_t **newpath, int path_delta) {

  int len = 128, done;
  do {
    *oldpath = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(len));
    if (!*oldpath) return;
    done = GetModuleFileNameW(NULL, *oldpath, len);
    if (done == 0) return;
    if (done >= len-1) {
      len <<= 1;
      HeapFree(GetProcessHeap(), 0, oldpath);
      done = 0;
    }
  } while (!done);
  *newpath=HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(len + path_delta));
}


__inline wchar_t *find_last_slash_or_backslash_delimiter(wchar_t *start) {
  wchar_t *delimiter = start + lstrlenW(start);
  while (delimiter>start && *delimiter != L'/' && *delimiter != L'\\')
    --delimiter;
  return delimiter;
}


__inline wchar_t *move_delimiter_to_the_root_of_toolchain(wchar_t *delimiter,
                                                          int *nacl64) {
  if ((delimiter[-4] == L'/' || delimiter[-4] == L'\\') &&
      (delimiter[-3] == L'b' || delimiter[-3] == L'B') &&
      (delimiter[-2] == L'i' || delimiter[-2] == L'I') &&
      (delimiter[-1] == L'n' || delimiter[-1] == L'N') &&
      (delimiter[1] == L'n' || delimiter[1] == L'N') &&
      (delimiter[2] == L'a' || delimiter[2] == L'A') &&
      (delimiter[3] == L'c' || delimiter[3] == L'C') &&
      (delimiter[4] == L'l' || delimiter[4] == L'L') &&
       delimiter[5] == L'-') {
    delimiter -= 4;
    *nacl64 = 0;
  } else if ((delimiter[-4] == L'/' || delimiter[-4] == L'\\') &&
           (delimiter[-3] == L'b' || delimiter[-3] == L'B') &&
           (delimiter[-2] == L'i' || delimiter[-2] == L'I') &&
           (delimiter[-1] == L'n' || delimiter[-1] == L'N') &&
           (delimiter[1] == L'n' || delimiter[1] == L'N') &&
           (delimiter[2] == L'a' || delimiter[2] == L'A') &&
           (delimiter[3] == L'c' || delimiter[3] == L'C') &&
           (delimiter[4] == L'l' || delimiter[4] == L'L') &&
           (delimiter[5] == L'6' || delimiter[5] == L'6') &&
           (delimiter[6] == L'4' || delimiter[6] == L'4') &&
            delimiter[7] == L'-') {
    delimiter -= 4;
    *nacl64 = 1;
  } else if ((delimiter[-9] == L'/' || delimiter[-9] == L'\\') &&
             (delimiter[-8] == L'n' || delimiter[-8] == L'N') &&
             (delimiter[-7] == L'a' || delimiter[-7] == L'A') &&
             (delimiter[-6] == L'c' || delimiter[-6] == L'C') &&
             (delimiter[-5] == L'l' || delimiter[-5] == L'L') &&
             (delimiter[-4] == L'/' || delimiter[-4] == L'\\') &&
             (delimiter[-3] == L'b' || delimiter[-3] == L'B') &&
             (delimiter[-2] == L'i' || delimiter[-2] == L'I') &&
             (delimiter[-1] == L'n' || delimiter[-1] == L'N')) {
    delimiter -= 9;
    *nacl64 = 0;
  } else if ((delimiter[-11] == L'/' || delimiter[-11] == L'\\') &&
             (delimiter[-10] == L'n' || delimiter[-10] == L'N') &&
             (delimiter[-9] == L'a' || delimiter[-9] == L'A') &&
             (delimiter[-8] == L'c' || delimiter[-8] == L'C') &&
             (delimiter[-7] == L'l' || delimiter[-7] == L'L') &&
             (delimiter[-6] == L'6' || delimiter[-6] == L'6') &&
             (delimiter[-5] == L'4' || delimiter[-5] == L'4') &&
             (delimiter[-4] == L'/' || delimiter[-4] == L'\\') &&
             (delimiter[-3] == L'b' || delimiter[-3] == L'B') &&
             (delimiter[-2] == L'i' || delimiter[-2] == L'I') &&
             (delimiter[-1] == L'n' || delimiter[-1] == L'N')) {
    delimiter -= 11;
    *nacl64 = 1;
  } else {
    *nacl64 = 2;
  }
  return delimiter;
}

__inline wchar_t *contrive_full_name_of_executable(wchar_t *newpath,
                                                   wchar_t *oldpath,
                                                   wchar_t **delimiter,
                                                   wchar_t **shortname,
                                                   int nacl64) {
  int done = *delimiter - oldpath;
  lstrcpynW(newpath, oldpath, done + 1);
  if (nacl64) {
    lstrcpyW(newpath + done, L"\\libexec\\nacl64-");
    *shortname = newpath + done + 9;
    done += 16;
    *delimiter += 12;
  } else {
    lstrcpyW(newpath + done, L"\\libexec\\nacl-");
    *shortname = newpath + done + 9;
    done += 14;
    *delimiter += 10;
  }
  lstrcpynW(newpath + done, *delimiter, 32);
  return newpath;
}


__inline wchar_t *adjust_selector_by_program_name(wchar_t *selector,
                                                  wchar_t *program_name,
                                                  wchar_t *cmdline,
                                                  wchar_t *fullpath) {
  /*
   * For as we use --32/--64,
   * for 32bit ld it's -melf_nacl,
   * for gcc it's -m32/-m64
   */
  if ((program_name[0] == L'a' || program_name[0] == L'A') &&
      (program_name[1] == L's' || program_name[0] == L'S') &&
      (program_name[2] == '.')) {
    selector[2] = '-';
  } else if ((program_name[0] == L'l' || program_name[0] == L'L') &&
           (program_name[1] == L'd' || program_name[0] == L'D') &&
           (program_name[2] == '.') && (selector[3] == '3')) {
    lstrcpyW(selector + 2, L"melf_nacl");
  } else if (((program_name[0] == L'c' || program_name[0] == L'C') &&
            (program_name[1] == L'+') && (program_name[2] == L'+') &&
            (program_name[3] == '.')) ||
           ((program_name[0] == L'c' || program_name[0] == L'C') &&
            (program_name[1] == L'p' || program_name[1] == L'P') &&
            (program_name[2] == L'p' || program_name[2] == L'P') &&
            (program_name[3] == '.')) ||
           ((program_name[0] == L'g' || program_name[0] == L'G') &&
            (program_name[1] == L'+') && (program_name[2] == L'+') &&
            (program_name[3] == '.')) ||
           ((program_name[0] == L'g' || program_name[0] == L'G') &&
            (program_name[1] == L'c' || program_name[1] == L'C') &&
            (program_name[2] == L'c' || program_name[2] == L'C') &&
            (program_name[3] == '.') || (program_name[3] == '-')) ||
           ((program_name[0] == L'g' || program_name[0] == L'G') &&
            (program_name[1] == L'f' || program_name[1] == L'F') &&
            (program_name[2] == L'o' || program_name[2] == L'O') &&
            (program_name[3] == L'r' || program_name[3] == L'R') &&
            (program_name[4] == L't' || program_name[4] == L'T') &&
            (program_name[5] == L'r' || program_name[5] == L'R') &&
            (program_name[6] == L'a' || program_name[6] == L'A') &&
            (program_name[7] == L'n' || program_name[7] == L'N') &&
            (program_name[8] == '.'))) {
    selector[2] = 'm';
    return fullpath;
  }
  return cmdline;
}


__inline wchar_t *skip_program_name_and_find_program_arguments() {
  wchar_t *arguments;

  arguments = GetCommandLineW();
  if (!arguments) return NULL;
  if (arguments[0] == L'\"') {
    arguments++;
    while (arguments[0] && arguments[0] != L'\"') {
      if (arguments[0] == L'\\' &&
          (arguments[1] == L'\"' || arguments[1] == L'\\'))
        arguments++;
      arguments++;
    }
    arguments++;
  }
  while (arguments[0] && arguments[0] != L' ') arguments++;
  if (arguments[0]) while (arguments[1] == ' ') arguments++;
  return arguments;
}


void entry() {
  wchar_t selector[] = { L' ', L'-', 0, L'3', L'2', 0, 0, 0, 0, 0, 0, 0 };
  wchar_t *newpath = NULL, *oldpath = NULL, *cmdline, *delimiter;
  int nacl64, len, done;
  static STARTUPINFOW si = { sizeof(STARTUPINFOW), 0};
  static PROCESS_INFORMATION pi;
  get_module_name_safely_and_allocate_buffer_for_rewrite(
     &oldpath, &newpath, 64);
  if (!oldpath || !newpath) goto ShowErrorMessage;
  delimiter = find_last_slash_or_backslash_delimiter(oldpath);
  if (delimiter == oldpath) goto ShowErrorMessage;
  delimiter = move_delimiter_to_the_root_of_toolchain(delimiter, &nacl64);
  if (nacl64 == 2) goto ShowErrorMessage;
  contrive_full_name_of_executable(
    newpath, oldpath, &delimiter, &cmdline, nacl64);
  if (nacl64) {
    selector[3] = L'6';
    selector[4] = L'4';
  }
  cmdline = adjust_selector_by_program_name(selector, delimiter,
                                            cmdline, newpath);
  HeapFree(GetProcessHeap(), 0, oldpath);
  oldpath = NULL;
  delimiter = skip_program_name_and_find_program_arguments();
  if (!delimiter) goto ShowErrorMessage;
  len = lstrlenW(cmdline);
  done = 0;
  if (selector[2]) done = lstrlenW(selector);
  oldpath = HeapAlloc(GetProcessHeap(), 0,
                      (lstrlenW(delimiter) + len + done + 4) * sizeof(wchar_t));
  if (!oldpath) goto ShowErrorMessage;
  lstrcpyW(oldpath, cmdline);
  if (cmdline == newpath && delimiter[1] == L'-' && delimiter[2] == L'V') {
    oldpath[len++] = (delimiter++)[0];
    oldpath[len++] = (delimiter++)[0];
    oldpath[len++] = (delimiter++)[0];
    if (delimiter[0] == L' ')
      while (delimiter[1] == ' ')
        delimiter++;
    oldpath[len++] = (delimiter++)[0];
    while (delimiter[0] && delimiter[0] != L' ')
      oldpath[len++] = (delimiter++)[0];
  }
  lstrcpyW(oldpath + len, selector);
  lstrcpyW(oldpath + len + done, delimiter);
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
  if (newpath) HeapFree(GetProcessHeap(), 0, newpath);
  if (oldpath) HeapFree(GetProcessHeap(), 0, oldpath);
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), FILE_NOT_FOUND_ERROR_MESSAGE,
            sizeof(FILE_NOT_FOUND_ERROR_MESSAGE) - 1, &len, NULL);
}
