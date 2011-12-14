/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/osx/crash_filter.h"

#include <mach/mach.h>
#include <mach/task.h>

#include "native_client/src/trusted/service_runtime/sel_rt.h"


/*
 * We could provide a version for x86-64, but it would not get tested
 * because we run only minimal tests for x86-64 Mac.  This function is
 * currently only used in Chromium which only uses x86-32 NaCl on Mac.
 */
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32

int NaClMachThreadIsInUntrusted(mach_port_t thread_port) {
  natural_t regs_array[i386_THREAD_STATE_COUNT];
  mach_msg_type_number_t size = NACL_ARRAY_SIZE(regs_array);
  i386_thread_state_t *regs = (i386_thread_state_t *) regs_array;
  kern_return_t rc;

  rc = thread_get_state(thread_port, i386_THREAD_STATE, regs_array, &size);
  if (rc != 0) {
    NaClLog(LOG_FATAL, "NaClMachThreadIsInUntrusted: "
            "thread_get_state() failed with error %i\n", (int) rc);
  }

  return regs->__cs != NaClGetGlobalCs();
}

#endif
