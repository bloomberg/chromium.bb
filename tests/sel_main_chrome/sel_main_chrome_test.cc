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
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
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

int main(int argc, char **argv) {
  struct NaClChromeMainArgs *args = NaClChromeMainArgsCreate();
  struct ThreadArgs thread_args;

  NaClAllModulesInit();

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

  NaClThread thread;
  CHECK(NaClThreadCtor(&thread, DummyRendererThread, &thread_args,
                       NACL_KERN_STACK_SIZE));

  NaClChromeMainStart(args);
  NaClLog(LOG_FATAL, "NaClChromeMainStart() should never return\n");
  return 1;
}
