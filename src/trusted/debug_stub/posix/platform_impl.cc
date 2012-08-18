/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>

#include <map>
#include <vector>

#include "native_client/src/include/concurrency_ops.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/util.h"
#include "native_client/src/trusted/debug_stub/platform.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


/*
 * Define the OS specific portions of IPlatform interface.
 */


namespace port {

void IPlatform::Relinquish(uint32_t msec) {
  usleep(msec * 1000);
}

// In order to read from a pointer that might not be valid, we use the
// trick of getting the kernel to do it on our behalf.
static bool SafeMemoryCopy(void *dest, void *src, size_t len) {
  // The trick only works if we are copying less than the buffer size
  // of a pipe.  For now, return an error on larger sizes.
  // TODO(mseaborn): If we need to copy more, we would have to break
  // it up into smaller parts.
  const size_t kPipeBufferBound = 0x1000;
  if (len > kPipeBufferBound)
    return false;

  bool success = false;
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0)
    return false;
  ssize_t sent = write(pipe_fds[1], src, len);
  if (sent == static_cast<ssize_t>(len)) {
    ssize_t got = read(pipe_fds[0], dest, len);
    if (got == static_cast<ssize_t>(len))
      success = true;
  }
  CHECK(close(pipe_fds[0]) == 0);
  CHECK(close(pipe_fds[1]) == 0);
  return success;
}

bool IPlatform::GetMemory(uint64_t virt, uint32_t len, void *dst) {
  return SafeMemoryCopy(dst, reinterpret_cast<void*>(virt), len);
}

bool IPlatform::SetMemory(uint64_t virt, uint32_t len, void *src) {
  uintptr_t page_mask = NACL_PAGESIZE - 1;
  uintptr_t page = virt & ~page_mask;
  uintptr_t mapping_size = ((virt + len + page_mask) & ~page_mask) - page;
  if (mprotect(reinterpret_cast<void*>(page), mapping_size,
               PROT_READ | PROT_WRITE) != 0) {
    return false;
  }
  bool succeeded = SafeMemoryCopy(reinterpret_cast<void*>(virt), src, len);
  // TODO(mseaborn): We assume here that SetMemory() is being used to
  // set or remove a breakpoint in the code area, so that PROT_READ |
  // PROT_EXEC are the correct flags to restore the mapping to.
  // The earlier mprotect() does not tell us what the original flags
  // were.  To find this out we could either:
  //  * read /proc/self/maps (not available inside outer sandbox); or
  //  * use service_runtime's own mapping tables.
  // Alternatively, we could modify code the same way nacl_text.c does.
  if (mprotect(reinterpret_cast<void*>(page), mapping_size,
               PROT_READ | PROT_EXEC) != 0) {
    return false;
  }
  // Flush the instruction cache in case we just modified code to add
  // or remove a breakpoint, otherwise breakpoints will not behave
  // reliably on ARM.
  NaClFlushCacheForDoublyMappedCode((uint8_t *) virt, (uint8_t *) virt, len);
  return succeeded;
}

}  // End of port namespace

