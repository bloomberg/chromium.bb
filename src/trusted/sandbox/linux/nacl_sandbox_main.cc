/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/sandbox/linux/nacl_sandbox.h"

const int kMaxPath = 1024;

int main(int argc, char* argv[]) {
  if (!NaclSandbox::SelLdrPath(argc, argv, argv[0], strlen(argv[0]))) {
    return 1;
  }
  // TODO(neha): This is off by default for now.  Change it to an
  // environment variable which must be set in order to *not* run the
  // sandbox.
  if (getenv(NaclSandbox::kTraceSandboxEnvVariable) == NULL) {
    // Fall through.  No sandbox.
    printf("Warning: starting untraced sel_ldr: %s \n", argv[0]);
    execv(argv[0], argv);
    perror("exec");
    exit(-20);
  }

  char app_name[kMaxPath];
  char* app_ptr = app_name;
  if (!NaclSandbox::ParseApplicationName(argc, argv, app_ptr, kMaxPath)) {
    return 1;
  }

  NaclSandbox::NaclSandbox sandbox;
  sandbox.Run(app_ptr, argv);
  return 0;
}
