/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_c.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

using std::vector;

extern "C" {
struct NaClSelLdrLauncher* NaClSelLdrStart(const char* application_name,
                                           int imc_fd,
                                           int sel_ldr_argc,
                                           const char* sel_ldr_argv[],
                                           int app_argc,
                                           const char* app_argv[]) {
  vector<nacl::string> sel_ldr_vec(sel_ldr_argv, sel_ldr_argv + sel_ldr_argc);
  vector<nacl::string> app_vec(app_argv, app_argv + app_argc);

  nacl::SelLdrLauncher* launcher = new nacl::SelLdrLauncher();
  if (launcher->Start(application_name,
                      imc_fd,
                      sel_ldr_vec,
                      app_vec)) {
    return reinterpret_cast<struct NaClSelLdrLauncher*>(launcher);
  } else {
    delete launcher;
    return NULL;
  }
}

// Helper to centralize application of ugly cast.
static nacl::SelLdrLauncher* ToObject(struct NaClSelLdrLauncher* launcher) {
  return reinterpret_cast<struct nacl::SelLdrLauncher*>(launcher);
}


void NaClSelLdrShutdown(struct NaClSelLdrLauncher* launcher) {
  delete ToObject(launcher);
}


int NaClSelLdrOpenSrpcChannels(struct NaClSelLdrLauncher* launcher,
                               struct NaClSrpcChannel* command,
                               struct NaClSrpcChannel* untrusted_command,
                               struct NaClSrpcChannel* untrusted) {
  return ToObject(launcher)->
    OpenSrpcChannels(command, untrusted_command, untrusted);
}


NaClHandle NaClSelLdrGetChild(struct NaClSelLdrLauncher* launcher) {
  return ToObject(launcher)->child();
}


NaClHandle NaClSelLdrGetChannel(struct NaClSelLdrLauncher* launcher) {
  return ToObject(launcher)->channel();
}


struct NaClDesc* NaClSelLdrGetSockAddr(struct NaClSelLdrLauncher* launcher) {
  return ToObject(launcher)->GetSelLdrSocketAddress();
}
}  // extern "C"
