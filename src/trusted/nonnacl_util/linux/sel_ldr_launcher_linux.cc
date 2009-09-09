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
