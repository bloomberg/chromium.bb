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
 * The toolchain redirector reads it's name and invokes the appropriate binary
 * from a location with cygwin1.dll nearby so that all cygwin-oriented
 * toolchain programs can start (this works for Cygwin version >= 1.7).
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

/*
 * Wraps GetModuleFileNameW to always succeed in finding a buffer of
 * appropriate size.
 */
static int get_module_name_safely(wchar_t **oldpath) {
  int length = 128, done;
  do {
    *oldpath = HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(wchar_t)*(length));
    if (!*oldpath) return 0;
    done = GetModuleFileNameW(NULL, *oldpath, length);
    if (!done) return 0;
    if (done >= length - 1) {
      length <<= 1;
      HeapFree(GetProcessHeap(), 0, oldpath);
      done = 0;
    }
  } while (!done);
  return length;
}


static wchar_t* find_last_slash_or_backslash_delimiter(wchar_t *start) {
  wchar_t *delimiter = start + lstrlenW(start);
  while (delimiter > start && *delimiter != L'/' && *delimiter != L'\\')
    --delimiter;
  return delimiter;
}


static int
move_delimiter_to_root_of_toolchain_return_toolchain_type_where_2_means_error(
    wchar_t **delimiter) {
  if (((*delimiter)[-4] == L'/' || (*delimiter)[-4] == L'\\') &&
      ((*delimiter)[-3] == L'b' || (*delimiter)[-3] == L'B') &&
      ((*delimiter)[-2] == L'i' || (*delimiter)[-2] == L'I') &&
      ((*delimiter)[-1] == L'n' || (*delimiter)[-1] == L'N') &&
      ((*delimiter)[1] == L'n' || (*delimiter)[1] == L'N') &&
      ((*delimiter)[2] == L'a' || (*delimiter)[2] == L'A') &&
      ((*delimiter)[3] == L'c' || (*delimiter)[3] == L'C') &&
      ((*delimiter)[4] == L'l' || (*delimiter)[4] == L'L') &&
       (*delimiter)[5] == L'-') {
    (*delimiter) -= 4;
    return 0;
  } else if (((*delimiter)[-4] == L'/' || (*delimiter)[-4] == L'\\') &&
           ((*delimiter)[-3] == L'b' || (*delimiter)[-3] == L'B') &&
           ((*delimiter)[-2] == L'i' || (*delimiter)[-2] == L'I') &&
           ((*delimiter)[-1] == L'n' || (*delimiter)[-1] == L'N') &&
           ((*delimiter)[1] == L'n' || (*delimiter)[1] == L'N') &&
           ((*delimiter)[2] == L'a' || (*delimiter)[2] == L'A') &&
           ((*delimiter)[3] == L'c' || (*delimiter)[3] == L'C') &&
           ((*delimiter)[4] == L'l' || (*delimiter)[4] == L'L') &&
           ((*delimiter)[5] == L'6' || (*delimiter)[5] == L'6') &&
           ((*delimiter)[6] == L'4' || (*delimiter)[6] == L'4') &&
            (*delimiter)[7] == L'-') {
    (*delimiter) -= 4;
    return 1;
  } else if (((*delimiter)[-9] == L'/' || (*delimiter)[-9] == L'\\') &&
             ((*delimiter)[-8] == L'n' || (*delimiter)[-8] == L'N') &&
             ((*delimiter)[-7] == L'a' || (*delimiter)[-7] == L'A') &&
             ((*delimiter)[-6] == L'c' || (*delimiter)[-6] == L'C') &&
             ((*delimiter)[-5] == L'l' || (*delimiter)[-5] == L'L') &&
             ((*delimiter)[-4] == L'/' || (*delimiter)[-4] == L'\\') &&
             ((*delimiter)[-3] == L'b' || (*delimiter)[-3] == L'B') &&
             ((*delimiter)[-2] == L'i' || (*delimiter)[-2] == L'I') &&
             ((*delimiter)[-1] == L'n' || (*delimiter)[-1] == L'N')) {
    (*delimiter) -= 9;
    return 0;
  } else if (((*delimiter)[-11] == L'/' || (*delimiter)[-11] == L'\\') &&
             ((*delimiter)[-10] == L'n' || (*delimiter)[-10] == L'N') &&
             ((*delimiter)[-9] == L'a' || (*delimiter)[-9] == L'A') &&
             ((*delimiter)[-8] == L'c' || (*delimiter)[-8] == L'C') &&
             ((*delimiter)[-7] == L'l' || (*delimiter)[-7] == L'L') &&
             ((*delimiter)[-6] == L'6' || (*delimiter)[-6] == L'6') &&
             ((*delimiter)[-5] == L'4' || (*delimiter)[-5] == L'4') &&
             ((*delimiter)[-4] == L'/' || (*delimiter)[-4] == L'\\') &&
             ((*delimiter)[-3] == L'b' || (*delimiter)[-3] == L'B') &&
             ((*delimiter)[-2] == L'i' || (*delimiter)[-2] == L'I') &&
             ((*delimiter)[-1] == L'n' || (*delimiter)[-1] == L'N')) {
    (*delimiter) -= 11;
    return 1;
  }
  return 2;
}

static wchar_t* adjust_selector_by_program_name(wchar_t *selector,
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
    /*
     * Note: for gcc drivers we return fullpath, for normal programs - short
     * cmdline only. This is because GCC strips path from command line itself
     * but uses path component to find cc1/collect2/etc while as/ld/etc don't
     * strip path and don't need the name to search for subcomponents.
     */
    return fullpath;
  }
  return cmdline;
}


static wchar_t* find_program_arguments() {
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
  wchar_t *newpath = NULL, *oldpath = NULL;
  wchar_t *cmdline, *delimiter, *program_name, *arguments;
  int is_tool64, length, done;
  static STARTUPINFOW si = { sizeof(STARTUPINFOW), 0};
  static PROCESS_INFORMATION pi;

  length = get_module_name_safely(&oldpath);
  newpath = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(length + 64));
  if (!oldpath || !newpath) goto ShowErrorMessage;
  delimiter = find_last_slash_or_backslash_delimiter(oldpath);
  if (delimiter == oldpath) goto ShowErrorMessage;
  is_tool64 =
  move_delimiter_to_root_of_toolchain_return_toolchain_type_where_2_means_error
    (&delimiter);
  if (is_tool64 == 2) goto ShowErrorMessage;
  done = delimiter - oldpath;
  lstrcpynW(newpath, oldpath, done + 1);
  if (is_tool64) {
    selector[3] = L'6';
    selector[4] = L'4';
    lstrcpyW(newpath + done, L"\\libexec\\nacl64-");
    /* This is short cmdline. Most programs need short name. */
    cmdline = newpath + done + 9;
    done += 16;
    program_name = delimiter + 12;
  } else {
    lstrcpyW(newpath + done, L"\\libexec\\nacl-");
    /* This is short cmdline. Most programs need short name. */
    cmdline = newpath + done + 9;
    done += 14;
    program_name = delimiter + 10;
  }
  lstrcpynW(newpath + done, program_name, 32);
  cmdline = adjust_selector_by_program_name(selector, program_name,
                                            cmdline, newpath);
  HeapFree(GetProcessHeap(), 0, oldpath);
  oldpath = NULL;
  arguments = find_program_arguments();
  if (!arguments) goto ShowErrorMessage;
  length = lstrlenW(cmdline);
  done = 0;
  if (selector[2]) done = lstrlenW(selector);
  oldpath = HeapAlloc(GetProcessHeap(), 0,
    (lstrlenW(arguments) + length + done + 4) * sizeof(wchar_t));
  if (!oldpath) goto ShowErrorMessage;
  lstrcpyW(oldpath, cmdline);
  /*
   * cmdline == newpath means it's some kind of gcc driver and we need to
   * handle this case specially: we need to put the -V option to be the first.
   */
  if (cmdline == newpath && arguments[1] == L'-' && arguments[2] == L'V') {
    oldpath[length++] = (arguments++)[0];
    oldpath[length++] = (arguments++)[0];
    oldpath[length++] = (arguments++)[0];
    if (arguments[0] == L' ')
      while (arguments[1] == ' ')
        arguments++;
    oldpath[length++] = (arguments++)[0];
    while (arguments[0] && arguments[0] != L' ')
      oldpath[length++] = (arguments++)[0];
  }
  lstrcpyW(oldpath + length, selector);
  lstrcpyW(oldpath + length + done, arguments);
  if (CreateProcessW(newpath, oldpath, NULL, NULL, TRUE,
                     CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi) != 0) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    HeapFree(GetProcessHeap(), 0, newpath);
    HeapFree(GetProcessHeap(), 0, oldpath);
    if (GetExitCodeProcess(pi.hProcess, &done)) {
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
      ExitProcess(done);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
ShowErrorMessage:
  if (newpath) HeapFree(GetProcessHeap(), 0, newpath);
  if (oldpath) HeapFree(GetProcessHeap(), 0, oldpath);
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), FILE_NOT_FOUND_ERROR_MESSAGE,
            sizeof(FILE_NOT_FOUND_ERROR_MESSAGE) - 1, &length, NULL);
  ExitProcess(1);
}
