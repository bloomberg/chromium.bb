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


#include <stdio.h>
#include <sys/types.h>

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_c.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

extern "C" {
struct NaClSelLdrLauncher* NaClSelLdrStart(const char* application_name,
                                           int imc_fd,
                                           int sel_ldr_argc,
                                           const char* sel_ldr_argv[],
                                           int application_argc,
                                           const char* application_argv[]) {
  nacl::SelLdrLauncher* launcher =
      new nacl::SelLdrLauncher();
  if (launcher->Start(application_name,
                      imc_fd,
                      sel_ldr_argc,
                      sel_ldr_argv,
                      application_argc,
                      application_argv)) {
    return reinterpret_cast<struct NaClSelLdrLauncher*>(launcher);
  } else {
    delete launcher;
    return NULL;
  }
}

void NaClSelLdrShutdown(struct NaClSelLdrLauncher* launcher) {
  delete reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher);
}

int NaClSelLdrOpenSrpcChannels(struct NaClSelLdrLauncher* launcher,
                               struct NaClSrpcChannel* command,
                               struct NaClSrpcChannel* untrusted_command,
                               struct NaClSrpcChannel* untrusted) {
  nacl::SelLdrLauncher* cpp_launcher =
      reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher);
  return cpp_launcher->OpenSrpcChannels(command, untrusted_command, untrusted);
}

NaClHandle NaClSelLdrGetChild(struct NaClSelLdrLauncher* launcher) {
  return (reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher))->child_;
}
NaClHandle NaClSelLdrGetChannel(struct NaClSelLdrLauncher* launcher) {
  return (reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher))->channel_;
}


}  // extern "C"
