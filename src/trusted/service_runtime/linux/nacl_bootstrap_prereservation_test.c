/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

/*
 * FindMinUnreservedAddress returns the smallest unrereserved address
 * in the address space above 64K of the process running this program
 * by examining the file /proc/self/maps.
 */
size_t FindMinUnreservedAddress() {
  FILE *fp;
  char buf[256];
  int result;
  size_t last_end;

  fp = fopen("/proc/self/maps", "r");
  CHECK(fp != NULL);

  /*
   * Scan the maps file for the first address that is unreserved.  The
   * first address is at 64K.
   */
  last_end = 0x10000;
  while (fgets(buf, sizeof(buf), fp)) {
    size_t start;
    size_t end;
    result = sscanf(buf, "%zx-%zx %*4[-rwxp] %*8x %*2x:%*2x %*16x", &start,
                    &end);
    if (result != 2 || start != last_end)
      break;
    last_end = end;
  }
  fclose(fp);
  return last_end;
}

/*
 * This test checks that nacl_helper_bootstrap loads a program at
 * the correct location when prereserved sandbox memory is used.
 * The amount of prereserved memory is passed via the reserved_at_zero
 * command line option.
 */
int main(int argc, char **argv) {
  int match;
  static const char kRDebugSwitch[] = "--r_debug=";
  static const char kAtZeroSwitch[] = "--reserved_at_zero=";

  if (argc > 1 &&
      0 == strncmp(argv[1], kRDebugSwitch, sizeof(kRDebugSwitch) - 1)) {
    NaClHandleRDebug(&argv[1][sizeof(kRDebugSwitch) - 1], argv[0]);
    --argc;
    ++argv;
  }
  if (argc > 1 &&
      0 == strncmp(argv[1], kAtZeroSwitch, sizeof(kAtZeroSwitch) - 1)) {
    NaClHandleReservedAtZero(&argv[1][sizeof(kAtZeroSwitch) - 1]);
    --argc;
    ++argv;
  }

  /* On Linux x86-64, there is no prereserved memory. */
  if (g_prereserved_sandbox_size == 0) {
    printf("no prereserved sandbox memory\n");
    return 0;
  }

  /*
   * Check if g_prereserved_sandbox_size is less than or equal to the
   * first address which is not reserved in the address space.  If this
   * is the case, then enough memory was reserved.
   */
  match = (g_prereserved_sandbox_size <= FindMinUnreservedAddress());
  printf("%sable to reserve the right amount of memory\n", match ? "" : "not ");
  return !match;
}
