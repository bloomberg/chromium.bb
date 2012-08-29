/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>

#if NACL_WINDOWS
# include <io.h>
#endif

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_custom.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_main_chrome.h"


int OpenFileReadOnly(const char *filename) {
#if NACL_WINDOWS
  return _open(filename, _O_RDONLY);
#else
  return open(filename, O_RDONLY);
#endif
}

// This launcher class does not actually launch a process, but we
// reuse SelLdrLauncherBase in order to use its helper methods.
class DummyLauncher : public nacl::SelLdrLauncherBase {
 public:
  explicit DummyLauncher(NaClHandle channel) {
    channel_ = channel;
  }

  virtual bool Start(const char *url) {
    UNREFERENCED_PARAMETER(url);
    return true;
  }
};

struct ThreadArgs {
  NaClHandle channel;
  int nexe_fd;
};

void WINAPI DummyRendererThread(void *thread_arg) {
  struct ThreadArgs *args = (struct ThreadArgs *) thread_arg;

  nacl::DescWrapperFactory desc_wrapper_factory;
  nacl::DescWrapper *nexe_desc =
      desc_wrapper_factory.MakeFileDesc(args->nexe_fd, NACL_ABI_O_RDONLY);
  CHECK(nexe_desc != NULL);

  DummyLauncher launcher(args->channel);
  NaClSrpcChannel trusted_channel;
  NaClSrpcChannel untrusted_channel;
  CHECK(launcher.SetupCommandAndLoad(&trusted_channel, nexe_desc));
  CHECK(launcher.StartModuleAndSetupAppChannel(&trusted_channel,
                                               &untrusted_channel));
}

void ExampleDescDestroy(void *handle) {
  UNREFERENCED_PARAMETER(handle);
}

ssize_t ExampleDescSendMsg(void *handle,
                           const struct NaClImcTypedMsgHdr *msg,
                           int flags) {
  UNREFERENCED_PARAMETER(handle);
  UNREFERENCED_PARAMETER(msg);
  UNREFERENCED_PARAMETER(flags);

  NaClLog(LOG_FATAL, "ExampleDescSendMsg: Not implemented\n");
  return 0;
}

ssize_t ExampleDescRecvMsg(void *handle,
                           struct NaClImcTypedMsgHdr *msg,
                           int flags) {
  UNREFERENCED_PARAMETER(handle);
  UNREFERENCED_PARAMETER(msg);
  UNREFERENCED_PARAMETER(flags);

  NaClLog(LOG_FATAL, "ExampleDescRecvMsg: Not implemented\n");
  return 0;
}

struct NaClDesc *MakeExampleDesc() {
  struct NaClDescCustomFuncs funcs = NACL_DESC_CUSTOM_FUNCS_INITIALIZER;
  funcs.Destroy = ExampleDescDestroy;
  funcs.SendMsg = ExampleDescSendMsg;
  funcs.RecvMsg = ExampleDescRecvMsg;
  return NaClDescMakeCustomDesc(NULL, &funcs);
}

int main(int argc, char **argv) {
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  struct ThreadArgs thread_args;

  NaClHandleBootstrapArgs(&argc, &argv);
#if NACL_LINUX
  args->prereserved_sandbox_size = g_prereserved_sandbox_size;
#endif

  // Note that we deliberately do not call NaClAllModulesInit() here,
  // in order to mimic what we expect the Chromium side to do.

  CHECK(argc == 3);

  args->irt_fd = OpenFileReadOnly(argv[1]);
  CHECK(args->irt_fd >= 0);

  thread_args.nexe_fd = OpenFileReadOnly(argv[2]);
  CHECK(thread_args.nexe_fd >= 0);
  NaClFileNameForValgrind(argv[2]);

  NaClHandle socketpair[2];
  CHECK(NaClSocketPair(socketpair) == 0);
  args->imc_bootstrap_handle = socketpair[0];
  thread_args.channel = socketpair[1];

  // Check that NaClDescMakeCustomDesc() works when called before
  // NaClChromeMainStart().
  args->initial_ipc_desc = MakeExampleDesc();

  NaClThread thread;
  CHECK(NaClThreadCtor(&thread, DummyRendererThread, &thread_args,
                       NACL_KERN_STACK_SIZE));

  NaClChromeMainStart(args);
  NaClLog(LOG_FATAL, "NaClChromeMainStart() should never return\n");
  return 1;
}
