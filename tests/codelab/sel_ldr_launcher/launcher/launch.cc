/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#define NACL_LINUX 1

#include <stdio.h>
#include <string.h>
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

class PathSelLdrLocator : public nacl::SelLdrLocator {
  public:
    explicit PathSelLdrLocator(std::string path) : path_(path) { }

    virtual void GetDirectory(char* buffer, size_t len) {
      if (path_.size() >= len) {
        return;
      }
      strncpy(buffer, path_.c_str(), len);
    }

  private:
    const std::string path_;
};

bool ReallyStart(nacl::SelLdrLauncher* launcher,
    std::string &nacl_module) {
  // The arguments we want to pass to the service runtime are
  // "-P 5" sets the default SRPC channel to be over descriptor 5.  The 5 needs
  //      to match the 5 in the Launcher invocation below.
  // "-X 5" causes the service runtime to create a bound socket and socket
  //      address at descriptors 3 and 4.  The socket address is transferred as
  //      the first IMC message on descriptor 5.  This is used when connecting
  //      to socket addresses.
  // "-d" (not default) invokes the service runtime in debug mode.
  const char* kSelLdrArgs[] = { "-P", "5", "-X", "5" };
  const int kSelLdrArgLength = 4;
  std::vector<nacl::string> kArgv(kSelLdrArgs, kSelLdrArgs + kSelLdrArgLength);
  std::vector<nacl::string> kEmpty;

  if (NULL == launcher) {
    fprintf(stderr, "launcher == NULL\n");
    return false;
  }

  if (!launcher->Start(nacl_module.c_str(), 5, kArgv, kEmpty)) {
    fprintf(stderr, "Start failed\n");
    return false;
  }
  fprintf(stderr, "sel_ldr started. Now, need to establish a connection\n");
  return true;
}

void KillChild(nacl::SelLdrLauncher* launcher) {
  if (launcher->KillChild()) {
    fprintf(stderr, "Child killed\n");
  } else {
    fprintf(stderr, "Child was not killed\n");
  }
}

int main(int argc, char **argv) {
  NaClNrdAllModulesInit();
  if (argc != 3) {
    fprintf(stderr, "Usage: ./launch sel_ldr_dir nacl_module\n");
    return 1;
  }
  char* sel_ldr_dir = argv[1];
  std::string nacl_module = std::string(argv[2]);
  nacl::SelLdrLocator *locator = new PathSelLdrLocator(sel_ldr_dir);
  nacl::SelLdrLauncher* launcher = new nacl::SelLdrLauncher(locator);
  if (!ReallyStart(launcher, nacl_module)) {
    fprintf(stderr, "Not started\n");
  }

  NaClSrpcChannel* command = new NaClSrpcChannel();
  NaClSrpcChannel* untrusted_command = new NaClSrpcChannel();
  NaClSrpcChannel* untrusted = new NaClSrpcChannel();

  if (NULL == command || NULL == untrusted_command || NULL == untrusted) {
    fprintf(stderr, "Can't allocate memory for NaClSrpcChannel\n");
    delete command;
    delete untrusted_command;
    delete untrusted;
    KillChild(launcher);
    delete launcher;
    return 1;
  }

  fprintf(stderr, "Calling OpenSrpcChannels...\n");

  if (!launcher->OpenSrpcChannels(command, untrusted_command, untrusted)) {
    fprintf(stderr, "OpenSrpcChannels failed\n");
    fprintf(stderr, "Unable to load module: %s\n", nacl_module.c_str());
    delete command;
    delete untrusted_command;
    delete untrusted;
    KillChild(launcher);
    delete launcher;
    return 1;
  }

  fprintf(stderr, "Module %s is loaded successfully\n", nacl_module.c_str());

  fprintf(stderr, "Invoking GetGreeting...\n");
  const char* name = "Engineer";
  char* greeting;
  NaClSrpcError err_code = NaClSrpcInvokeBySignature(
      untrusted_command,
      "GetGreeting:s:s",
      name,
      &greeting);
  if (NACL_SRPC_RESULT_OK != err_code) {
    fprintf(stderr, "GetGreeting:s:s failed with error: %s",
        NaClSrpcErrorString(err_code));
  } else {
    fprintf(stderr, "GetGreeting returned: %s\n", greeting);
  }

  fprintf(stderr, "Shutting down...\n");
  delete command;
  delete untrusted_command;
  delete untrusted;
  KillChild(launcher);
  delete launcher;
  return 0;
}
