/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"


using std::vector;

namespace nacl {

SelLdrLauncherStandalone::~SelLdrLauncherStandalone() {
  CloseHandlesAfterLaunch();
  if (NACL_INVALID_HANDLE != child_process_) {
    int status;
    // Ensure child process (service runtime) is kaput.  NB: we might
    // close the command channel (or use the hard_shutdown RPC) rather
    // than killing the process to allow the service runtime to do
    // clean up, but the plugin should be responsible for that and we
    // shouldn't introduce any timeout wait in a dtor.  Currently,
    // ServiceRuntime::Shutdown kills the subprocess before closing
    // the command channel, so we aren't providing the opportunity for
    // a more graceful shutdown.
    KillChildProcess();
    waitpid(child_process_, &status, 0);
  }
}


nacl::string SelLdrLauncherStandalone::GetSelLdrPathName() {
  char buffer[FILENAME_MAX];
  GetPluginDirectory(buffer, sizeof(buffer));
  return nacl::string(buffer) + "/sel_ldr";
}

nacl::string SelLdrLauncherStandalone::GetSelLdrBootstrapPathName() {
#if NACL_LINUX
  char buffer[FILENAME_MAX];
  GetPluginDirectory(buffer, sizeof(buffer));
  return nacl::string(buffer) + "/nacl_helper_bootstrap";
#else
  return nacl::string(NACL_NO_FILE_PATH);
#endif
}

NaClHandle SelLdrLauncherStandalone::CreateBootstrapSocket(
    nacl::string* dest_fd) {
  NaClHandle pair[2];
  if (NaClSocketPair(pair) == -1) {
    return NACL_INVALID_HANDLE;
  }

  int rc = fcntl(pair[0], F_SETFD, FD_CLOEXEC);
  CHECK(rc == 0);
  close_after_launch_.push_back(pair[1]);

  *dest_fd = ToString(pair[1]);
  return pair[0];
}

const size_t kMaxExecArgs = 64;

bool SelLdrLauncherStandalone::StartViaCommandLine(
    const vector<nacl::string>& prefix,
    const vector<nacl::string>& sel_ldr_argv,
    const vector<nacl::string>& app_argv) {
  // Set up the command line.
  InitCommandLine(prefix, sel_ldr_argv, app_argv);
  // complete command line setup
  vector<nacl::string> command;
  BuildCommandLine(&command);
  if (kMaxExecArgs <= command.size()) {
    // TODO(robertm): emit error message
    return false;
  }
  // Set environment variable to keep the Mac sel_ldr from stealing the focus.
  // TODO(sehr): change this to use a command line parameter rather than env.
  setenv("NACL_LAUNCHED_FROM_BROWSER", "1", 0);
  // Fork the sel_ldr process.
  child_process_ = fork();
  if (child_process_ == -1) {
    return false;
  }

  if (child_process_ == 0) {
    // convert vector -> array assuming no more than kMaxArgs
    // NOTE: we also check this above so the assert should never fire
    assert(command.size() < kMaxExecArgs);
    const char* argv[kMaxExecArgs];
    for (size_t i = 0; i < command.size(); ++i) {
      argv[i] = command[i].c_str();
    }
    argv[command.size()] = NULL;

    execv(argv[0], const_cast<char**>(argv));
    NaClLog(LOG_ERROR, "execv failed, args were:\n");
    for (size_t i = 0; i < command.size(); ++i) {
      NaClLog(LOG_ERROR, "%s\n", argv[i]);
    }
    perror("execv");
    NaClExit(EXIT_FAILURE);
  }
  CloseHandlesAfterLaunch();
  return true;
}

bool SelLdrLauncherStandalone::KillChildProcess() {
  if (NACL_INVALID_HANDLE == child_process_) {
    // It is incorrect to use the kill syscall on NACL_INVALID_HANDLE
    // as the pid, since using -1 as pid is defined by POSIX.1-2001 to
    // send the signal (SIGKILL) to every process that the calling
    // process may send signals to (except for init), which is
    // Definitely Not What Was Intended for this.
    return true;
  }
  return 0 == kill(child_process_, SIGKILL);
  // We cannot set child_process_ to NACL_INVALID_HANDLE since we will
  // want to wait on its exit status.
}

}  // namespace nacl
