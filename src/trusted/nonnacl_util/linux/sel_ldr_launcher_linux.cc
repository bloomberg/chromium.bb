/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"

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
}

bool SelLdrLauncher::Start(const char* application_name,
                           int imc_fd,
                           int sel_ldr_argc,
                           const char* sel_ldr_argv[],
                           int application_argc,
                           const char* application_argv[]) {
  Handle pair[2];
  const char* const kNaclSelLdrBasename = "sel_ldr";
  const char* plugin_dirname = GetPluginDirname();
  char sel_ldr_path[MAXPATHLEN + 1];

  application_name_ = application_name;

  if (NULL == plugin_dirname) {
    return false;
  }
  // Uncomment to turn on the sandbox, or set this in your environment
  // TODO(neha):  Turn this on by default.
  //
  // setenv("NACL_ENABLE_OUTER_SANDBOX");
  snprintf(sel_ldr_path,
           sizeof(sel_ldr_path),
           "%s/%s",
           plugin_dirname,
           kNaclSelLdrBasename);

  if (SocketPair(pair) == -1) {
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
    Close(pair[0]);
    BuildArgv(sel_ldr_path,
              application_name,
              imc_fd,
              pair[1],
              sel_ldr_argc,
              sel_ldr_argv,
              application_argc,
              application_argv);
    execv(sel_ldr_path, const_cast<char **>(argv_));
    perror("exec");
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
