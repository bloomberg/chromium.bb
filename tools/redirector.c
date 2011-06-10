/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <wchar.h>
#include <windows.h>
#include "redirector.h"

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
 * Use the following commands:
 *   cl /c /O2 /GS- redirector.c
 *   link /subsystem:console /MERGE:.rdata=.text /NODEFAULTLIB
 *        redirector.obj kernel32.lib user32.lib
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

wchar_t reduce_wchar(wchar_t c) {
  if (c == L'/')
    return L'\\';
  return (wchar_t)CharLowerW((LPWSTR)c);
}

/*
 * Returns whether path ends with redirect
 */
int check_path (wchar_t *path, wchar_t *redirect) {
  int path_len = lstrlenW(path);
  int redirect_len = lstrlenW(redirect);
  int path_offset;
  int i;
  if (path_len < redirect_len)
    return 0;
  path_offset = path_len - redirect_len;
  for (i = 0; i < redirect_len; i++) {
    if (reduce_wchar(path[i+path_offset]) != reduce_wchar(redirect[i]))
      return 0;
  }
  return 1;
}

int is_driver(wchar_t *option) {
  return lstrcmpW(option, L"-m32") == 0 || lstrcmpW(option, L"-m64") == 0;
}

void println_wstring(wchar_t *str) {
  int length;
  int i;
  int tmp;
  HANDLE output;
  length = lstrlenW(str);
  output = GetStdHandle(STD_OUTPUT_HANDLE);
  for (i = 0; i < length; i++)
    WriteFile(output, str + i, 1, &tmp, NULL);
  WriteFile(output, "\n", 1, &tmp, NULL);
}

void entry() {
  wchar_t *newpath = NULL, *oldpath = NULL;
  wchar_t *cmdline, *arguments, *selector;
  int length, done;
  int redirect_index;
  int length_from;
  int length_to;
  int i;
  int tmp;
  HANDLE output;
  int n_redirects = sizeof(redirects)/sizeof(redirect_t);
  static STARTUPINFOW si = { sizeof(STARTUPINFOW), 0};
  static PROCESS_INFORMATION pi;

  length = get_module_name_safely(&oldpath);
  if (!oldpath) goto ShowErrorMessage;
  /* If redirector is called inside make file */
  if (check_path(oldpath, L"redirector.exe")) {
    output = GetStdHandle(STD_OUTPUT_HANDLE);
    for (redirect_index = 0;
         redirect_index < n_redirects;
         redirect_index++) {
      println_wstring(redirects[redirect_index][0]);
    }
    ExitProcess(0);
  }
  for (redirect_index = 0;
       redirect_index < n_redirects;
       redirect_index++) {
    if (check_path(oldpath, redirects[redirect_index][0]))
      break;
  }
  if (redirect_index >= n_redirects) goto ShowErrorMessage;
  newpath = HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t)*(length + 64));
  if (!newpath) goto ShowErrorMessage;
  length = lstrlenW(oldpath);
  length_from = lstrlenW(redirects[redirect_index][0]);
  length_to = lstrlenW(redirects[redirect_index][1]);
  lstrcpynW(newpath, oldpath, length - length_from + 1);
  lstrcpyW(newpath + length - length_from, redirects[redirect_index][1]);

  selector = redirects[redirect_index][2];
  cmdline = redirects[redirect_index][1];
  if (!is_driver(selector)) {
    cmdline = find_last_slash_or_backslash_delimiter (cmdline);
  } else {
    cmdline = newpath;
  }
  HeapFree(GetProcessHeap(), 0, oldpath);
  oldpath = NULL;
  arguments = find_program_arguments();
  if (!arguments) goto ShowErrorMessage;
  length = lstrlenW(cmdline);
  done = lstrlenW(selector);
  oldpath = HeapAlloc(GetProcessHeap(), 0,
    (lstrlenW(arguments) + length + done + 2) * sizeof(wchar_t));
  if (!oldpath) goto ShowErrorMessage;
  lstrcpyW(oldpath, cmdline);
  /*
   * cmdline == newpath means it's some kind of gcc driver and we need to
   * handle this case specially: we need to put the -V option to be the first.
   */
  if (is_driver(selector) && arguments[1] == L'-' && arguments[2] == L'V') {
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
  lstrcpyW(oldpath + length, L" ");
  lstrcpyW(oldpath + length + 1, selector);
  lstrcpyW(oldpath + length + done + 1, arguments);
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
