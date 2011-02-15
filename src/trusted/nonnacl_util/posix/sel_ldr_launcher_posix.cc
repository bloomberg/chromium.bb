/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"


using std::vector;

namespace nacl {

SelLdrLauncher::~SelLdrLauncher() {
  CloseHandlesAfterLaunch();
  if (kInvalidHandle != child_process_) {
    int status;
    waitpid(child_process_, &status, 0);
  }
  if (kInvalidHandle != channel_) {
    Close(channel_);
  }
}


nacl::string SelLdrLauncher::GetSelLdrPathName() {
  char buffer[FILENAME_MAX];
  GetPluginDirectory(buffer, sizeof(buffer));
  return nacl::string(buffer) + "/sel_ldr";
}

Handle SelLdrLauncher::ExportImcFD(int dest_fd) {
  Handle pair[2];
  if (SocketPair(pair) == -1) {
    return kInvalidHandle;
  }

  int rc = fcntl(pair[0], F_SETFD, FD_CLOEXEC);
  CHECK(rc == 0);
  close_after_launch_.push_back(pair[1]);

  sel_ldr_argv_.push_back("-i");
  sel_ldr_argv_.push_back(ToString(dest_fd) + ":" + ToString(pair[1]));
  return pair[0];
}

const size_t kMaxExecArgs = 64;

bool SelLdrLauncher::LaunchFromCommandLine() {
  // Uncomment to turn on the sandbox, or set this in your environment
  // TODO(neha):  Turn this on by default.
  //
  // setenv("NACL_ENABLE_OUTER_SANDBOX");

  if (channel_number_ != -1) {
    channel_ = ExportImcFD(channel_number_);
  }

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
    argv[command.size()] = 0;

    execv(sel_ldr_.c_str(), const_cast<char**>(argv));
    NaClLog(LOG_ERROR, "execv failed, args were:\n");
    for (size_t i = 0; i < command.size(); ++i) {
      NaClLog(LOG_ERROR, "%s\n", argv[i]);
    }
    perror("execv");
    _exit(EXIT_FAILURE);
  }
  CloseHandlesAfterLaunch();
  return true;
}

bool SelLdrLauncher::KillChildProcess() {
  return 0 == kill(child_process_, SIGKILL);
  // We cannot set child_process_ to kInvalidHandle since we will want to wait
  // on its exit status.
}

}  // namespace nacl
