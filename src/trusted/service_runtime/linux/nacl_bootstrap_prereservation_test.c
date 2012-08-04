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
#include "native_client/src/trusted/service_runtime/sel_util.h"

size_t GetProcSysVmMmapMinAddr() {
  FILE *fp;
  char buf[32];
  size_t min_addr = 0;
  /*
   * The first possible address is at vm.mmap_min_addr (which is usually at 64K
   * but is sometimes set lower), which can be found in
   * /proc/sys/vm/mmap_min_addr.
   */
  fp = fopen("/proc/sys/vm/mmap_min_addr", "r");
  CHECK(fp != NULL);
  if (fgets(buf, sizeof(buf), fp) &&
      sscanf(buf, "%zu", &min_addr) == 1) {
    /*
     * Sometimes, mmap_min_addr is not page-aligned, but we can only map on page
     * boundaries.  Thus, the minimum address is the first page boundary above
     * vm.mmap_min_addr.
     */
    min_addr = NaClRoundPage(min_addr);
  }
  fclose(fp);
  return min_addr;
}

/*
 * FindMinUnreservedAddress returns the smallest unrereserved address
 * in the address space above vm.mmap_min_addr of the process running this
 * program by examining the file /proc/self/maps.
 */
size_t FindMinUnreservedAddress() {
  FILE *fp;
  char buf[256];
  int result;
  size_t last_end = GetProcSysVmMmapMinAddr();

  fp = fopen("/proc/self/maps", "r");
  CHECK(fp != NULL);
  /* Scan the maps file for the first address that is unreserved. */
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

  NaClHandleBootstrapArgs(&argc, &argv);

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
