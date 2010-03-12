/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>
#include <windows.h>

#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

// Use a predefined symbol to get a handle to the current module.
// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
// @IGNORE_LINES_FOR_CODE_HYGIENE[1]
extern "C" IMAGE_DOS_HEADER __ImageBase;

using std::vector;

namespace nacl {

SelLdrLauncher::~SelLdrLauncher() {
  if (NULL != sock_addr_) {
    NaClDescUnref(sock_addr_);
  }
  if (kInvalidHandle != child_) {
    CloseHandle(child_);
  }
  if (kInvalidHandle != channel_) {
    Close(channel_);
  }
  if (NULL != sel_ldr_locator_) {
    delete sel_ldr_locator_;
  }
}

nacl::string SelLdrLauncher::GetSelLdrPathName() {
  char buffer[FILENAME_MAX];
#ifdef _WIN64
  const char* const kSelLdrBasename = "\\sel_ldr64.exe";
#else
  const char* const kSelLdrBasename = "\\sel_ldr.exe";
#endif
  GetPluginDirectory(buffer, sizeof(buffer));
  return nacl::string(buffer) + kSelLdrBasename;
}

// TODO(sehr): document what this is supposed to do exactly
// NOTE: this may be buggy, e.g. how is \\ handled?
static nacl::string Escape(nacl::string s) {
  nacl::string result;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '"') {
      result += "\\\"";
    } else if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '"') {
      result += "\\\\\\\"";
      ++i;
    } else {
      result.push_back(s[i]);
    }
  }

  return result;
}

bool SelLdrLauncher::Launch() {
  Handle pair[2];
  STARTUPINFOA startup_info;
  PROCESS_INFORMATION process_infomation;

  if (SocketPair(pair) == -1) {
    return false;
  }

  // Transfer pair[1] to child_ by an inheritance.
  Handle channel;
  if (!DuplicateHandle(GetCurrentProcess(), pair[1],
                       GetCurrentProcess(), &channel,
                       0, TRUE, DUPLICATE_SAME_ACCESS)) {
    Close(pair[0]);
    Close(pair[1]);
    return false;
  }

  InitChannelBuf(channel);
  vector<nacl::string> command;
  BuildArgv(&command);

  // Convert to single string for process creation.
  nacl::string str = "";
  for (size_t i = 0; i < command.size(); ++i) {
    if (i > 0) {
      str += " ";
    }
    str += "\"";
    str += Escape(command[i]);
    str += "\"";
  }

  memset(&startup_info, 0, sizeof startup_info);
  startup_info.cb = sizeof startup_info;
  memset(&process_infomation, 0, sizeof process_infomation);
  if (!CreateProcessA(NULL, const_cast<char*>(str.c_str()),
                      NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL,
                      &startup_info,
                      &process_infomation)) {
    Close(pair[0]);
    Close(pair[1]);
    Close(channel);
    return false;
  }

  Close(pair[1]);
  Close(channel);
  CloseHandle(process_infomation.hThread);
  child_ = process_infomation.hProcess;
  channel_ = pair[0];
  return true;
}

bool SelLdrLauncher::KillChild() {
    return 0 != TerminateProcess(child_, 9);
    // 9 is the exit code for the child_.  The value is actually not
    // material, since (currently) the launcher does not collect/report
    // it.
}

void PluginSelLdrLocator::GetDirectory(char* buffer, size_t len) {
  // __ImageBase is in the current module, which could be a .dll or .exe
  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  // NOTE: casting down to DWORD is safe the integer will become smaller
  //       at worst
  GetModuleFileNameA(this_module, buffer, static_cast<DWORD>(len));
  char* path_end = strrchr(buffer, '\\');
  if (NULL != path_end) {
    *path_end = '\0';
  }
}

}  // namespace nacl
