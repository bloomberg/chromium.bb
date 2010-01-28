/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */



#include <iostream>
#include <sstream>
#include <string.h>
#include <windows.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

// Use a predefined symbol to get a handle to the current module.
// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

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
}

bool SelLdrLauncher::Start(const char* application_name,
                           int imc_fd,
                           int sel_ldr_argc,
                           const char* sel_ldr_argv[],
                           int application_argc,
                           const char* application_argv[]) {
  Handle pair[2];
  STARTUPINFOA startup_info;
  PROCESS_INFORMATION process_infomation;
  const char* const kSelLdrBasename = "sel_ldr.exe";
  const char* plugin_dirname = GetPluginDirname();
  char sel_ldr_path[MAX_PATH];

  // TODO(sehr): this should be refactored to merge more with the Linux code.
  if (NULL == plugin_dirname) {
    plugin_dirname = "";
  }
  _snprintf_s(sel_ldr_path,
              sizeof(sel_ldr_path),
              sizeof(sel_ldr_path),
              "%s\\%s",
              plugin_dirname,
              kSelLdrBasename);

  application_name_ = application_name;

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

  // Build argc_ and argv_.
  BuildArgv(sel_ldr_path,
            application_name,
            imc_fd,
            channel,
            sel_ldr_argc,
            sel_ldr_argv,
            application_argc,
            application_argv);

  // Convert argv_ to a string for process creation.
  std::string str = "";
  for (int i = 0; i < argc_; ++i) {
    if (i > 0) {
      str += " ";
    }
    str += "\"";
    for (const char* p = argv_[i]; *p; ++p) {
      if (*p == '"') {
        str += "\\\"";
      } else if (*p == '\\' && p[1] == '"') {
        str += "\\\\\\\"";
        ++p;
      } else {
        str += *p;
      }
    }
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

// This function is not thread-safe and should be called from the main browser
// thread.
const char* SelLdrLauncher::GetPluginDirname() {
  static char system_buffer[MAX_PATH];

  // __ImageBase is in the current module, which could be a .dll or .exe
  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  GetModuleFileNameA(this_module, system_buffer, MAX_PATH);
  char* path_end = strrchr(system_buffer, '\\');
  if (NULL != path_end) {
    *path_end = '\0';
  }
  return system_buffer;
}

bool SelLdrLauncher::KillChild() {
  return 0 != TerminateProcess(child_, 9);
  // 9 is the exit code for the child_.  The value is actually not
  // material, since (currently) the launcher does not collect/report
  // it.
}

}  // namespace nacl
