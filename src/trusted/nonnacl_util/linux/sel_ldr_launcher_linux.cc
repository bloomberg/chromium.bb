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

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"


using std::string;
using std::vector;

namespace nacl {

SelLdrLauncher::~SelLdrLauncher() {
  // sock_addr_ is non-NULL only if the Start method successfully completes.
  if (NULL != sock_addr_) {
    NaClDescUnref(sock_addr_);
  }
  // Similarly, child_ is invalid unless Start successfully completes.
  if (kInvalidHandle != child_) {
    int status;
    waitpid(child_, &status, 0);
  }
  // Similarly, channel_ is invalid unless Start successfully completes.
  if (kInvalidHandle != channel_) {
    Close(channel_);
  }
  if (NULL != sel_ldr_locator_) {
    delete sel_ldr_locator_;
  }
}


string SelLdrLauncher::GetSelLdrPathName() {
  char buffer[FILENAME_MAX];
  GetPluginDirectory(buffer, sizeof(buffer));
  return string(buffer) + "/sel_ldr";
}

const size_t kMaxExecArgs = 64;

bool SelLdrLauncher::Launch() {
  Handle pair[2];
  // Uncomment to turn on the sandbox, or set this in your environment
  // TODO(neha):  Turn this on by default.
  //
  // setenv("NACL_ENABLE_OUTER_SANDBOX");
  if (SocketPair(pair) == -1) {
    return false;
  }

  // complete command line setup
  InitChannelBuf(pair[1]);
  vector<string> command;
  BuildArgv(&command);
  if (kMaxExecArgs <= command.size()) {
    // TODO(robertm): emit error message
    return false;
  }
  // Set environment variable to keep the Mac sel_ldr from stealing the focus.
  // TODO(sehr): change this to use a command line parameter rather than env.
  setenv("NACL_LAUNCHED_FROM_BROWSER", "1", 0);
  // Fork the sel_ldr process.
  child_ = fork();
  if (child_ == -1) {
    Close(pair[0]);
    Close(pair[1]);
    return false;
  }

  if (child_ == 0) {
    // convert vector -> array assuming no more than kMaxArgs
    // NOTE: we also check this above so the assert should never fire
    assert(command.size() < kMaxExecArgs);
    const char* argv[kMaxExecArgs];
    for (size_t i = 0; i < command.size(); ++i) {
      argv[i] = command[i].c_str();
    }
    argv[command.size()] = 0;

    Close(pair[0]);
    execv(sel_ldr_.c_str(), const_cast<char**>(argv));
    NaClLog(LOG_ERROR, "execv failed, args were:\n");
    for (size_t i = 0; i < command.size(); ++i) {
      NaClLog(LOG_ERROR, "%s\n", argv[i]);
    }
    perror("execv");
    _exit(EXIT_FAILURE);
  }
  Close(pair[1]);
  channel_ = pair[0];

  int flags = fcntl(channel_, F_GETFD);
  if (flags != -1) {
    flags |= FD_CLOEXEC;
    fcntl(channel_, F_SETFD, flags);
  }
  return true;
}

bool SelLdrLauncher::KillChild() {
  return 0 == kill(child_, SIGKILL);
  // We cannot set child_ to kInvalidHandle since we will want to wait
  // on its exit status.
}

}  // namespace nacl
