/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

SelLdrLauncherStandalone::~SelLdrLauncherStandalone() {
  CloseHandlesAfterLaunch();
  if (NACL_INVALID_HANDLE != child_process_) {
    // Ensure child process (service runtime) is kaput.  NB: we might
    // close the command channel (or use the hard_shutdown RPC) rather
    // than killing the process to allow the service runtime to do
    // clean up, but the plugin should be responsible for that and we
    // shouldn't introduce any timeout wait in a dtor.  Currently,
    // ServiceRuntime::Shutdown kills the subprocess before closing
    // the command channel, so we aren't providing the opportunity for
    // a more graceful shutdown.
    KillChildProcess();
    CloseHandle(child_process_);
  }
}

nacl::string SelLdrLauncherStandalone::GetSelLdrPathName() {
  char buffer[FILENAME_MAX];
  // Currently only the Chrome build uses the gyp-generated binaries,
  // and Chrome does not start sel_ldr by means of
  // SelLdrLauncherStandalone.  So we only refer to sel_ldr.exe here.
  const char* const kSelLdrBasename = "\\sel_ldr.exe";
  GetPluginDirectory(buffer, sizeof(buffer));
  return nacl::string(buffer) + kSelLdrBasename;
}

nacl::string SelLdrLauncherStandalone::GetSelLdrBootstrapPathName() {
  return nacl::string(NACL_NO_FILE_PATH);
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

NaClHandle SelLdrLauncherStandalone::CreateBootstrapSocket(
    nacl::string* dest_fd) {
  NaClHandle pair[2];
  if (NaClSocketPair(pair) == -1) {
    return NACL_INVALID_HANDLE;
  }

  // Transfer pair[1] to child_process_ by inheritance.
  // TODO(mseaborn): We could probably use SetHandleInformation() to
  // set HANDLE_FLAG_INHERIT rather than duplicating the handle and
  // closing the original.  But for now, we do the same as in
  // LaunchFromCommandLine() below.
  NaClHandle channel;
  if (!DuplicateHandle(GetCurrentProcess(), pair[1],
                       GetCurrentProcess(), &channel,
                       0, TRUE, DUPLICATE_SAME_ACCESS)) {
    NaClClose(pair[0]);
    NaClClose(pair[1]);
    return NACL_INVALID_HANDLE;
  }
  NaClClose(pair[1]);
  close_after_launch_.push_back(channel);

  *dest_fd = ToString(reinterpret_cast<uintptr_t>(channel));
  return pair[0];
}

bool SelLdrLauncherStandalone::StartViaCommandLine(
    const vector<nacl::string>& prefix,
    const vector<nacl::string>& sel_ldr_argv,
    const vector<nacl::string>& app_argv) {
  // Set up the command line.
  InitCommandLine(prefix, sel_ldr_argv, app_argv);
  STARTUPINFOA startup_info;
  PROCESS_INFORMATION process_infomation;

  vector<nacl::string> command;
  BuildCommandLine(&command);

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
                      NULL, NULL, TRUE, 0, NULL, NULL,
                      &startup_info,
                      &process_infomation)) {
    return false;
  }

  CloseHandlesAfterLaunch();
  CloseHandle(process_infomation.hThread);
  child_process_ = process_infomation.hProcess;
  return true;
}

bool SelLdrLauncherStandalone::KillChildProcess() {
  if (NACL_INVALID_HANDLE == child_process_)
    return false;
  return 0 != TerminateProcess(child_process_, 9);
  // 9 is the exit code for the child_process_.  The value is actually not
  // material, since (currently) the launcher does not collect/report it.
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
