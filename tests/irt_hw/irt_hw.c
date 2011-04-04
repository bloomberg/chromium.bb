/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Integrated Runtime Hello World.
 */
#include <string.h>
#include "native_client/src/untrusted/irt/irt.h"

#define HWString "Hello, Integrated Runtime\n"

int main(int argc, char *argv[]) {
  int num_errors = 0;
  const struct nacl_core *IRT =
    (const struct nacl_core *)nacl_getinterface(NACLIRTv0_1);

  if (IRT == NULL) return -1; /* can't call exit! */

  if (IRT->nacl_core_write(1, HWString, strlen(HWString)) !=
      strlen(HWString)) {
    num_errors += 1;
  }

  if (0 == num_errors) {
    IRT->nacl_core_exit(0);
  } else {
    IRT->nacl_core_exit(-1);
  }

  return 0;
}
