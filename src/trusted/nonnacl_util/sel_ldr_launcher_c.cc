/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

struct NaClDesc* NaClSelLdrGetSockAddr(struct NaClSelLdrLauncher* launcher) {
  struct nacl::SelLdrLauncher* obj =
      reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher);
  return obj->GetSelLdrSocketAddress();
}

}  // extern "C"
