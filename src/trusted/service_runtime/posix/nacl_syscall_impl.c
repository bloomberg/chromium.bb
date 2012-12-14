/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"

#include "native_client/src/trusted/service_runtime/linux/nacl_syscall_inl.h"


/*
 * TODO(bsy): REMOVE THIS AND PROVIDE GETRUSAGE.  This is normally
 * not a syscall; instead, it is a library routine on top of
 * getrusage, with appropriate clock tick translation.
 */
int32_t NaClSysClock(struct NaClAppThread *natp) {
  int32_t retval;

  NaClLog(3,
          ("Entered NaClSysClock(%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp);

  retval = clock();
  return retval;
}

int32_t NaClSysSysconf(struct NaClAppThread *natp,
                       int32_t name,
                       int32_t *result) {
  int32_t         retval = -NACL_ABI_EINVAL;
  static int32_t  number_of_workers = -1;
  int32_t         result_value;

  NaClLog(3,
          ("Entered NaClSysSysconf(%08"NACL_PRIxPTR
           "x, %d, 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, name, (uintptr_t) result);

  switch (name) {
#ifdef _SC_NPROCESSORS_ONLN
    case NACL_ABI__SC_NPROCESSORS_ONLN: {
      if (-1 == number_of_workers) {
        number_of_workers = sysconf(_SC_NPROCESSORS_ONLN);
      }
      if (-1 == number_of_workers) {
        /* failed to get the number of processors */
        retval = -NACL_ABI_EINVAL;
        goto cleanup;
      }
      result_value = number_of_workers;
      break;
    }
#endif
    case NACL_ABI__SC_SENDMSG_MAX_SIZE: {
      /* TODO(sehr,bsy): this value needs to be determined at run time. */
      const int32_t kImcSendMsgMaxSize = 1 << 16;
      result_value = kImcSendMsgMaxSize;
      break;
    }
    case NACL_ABI__SC_PAGESIZE: {
      result_value = 1 << 16;  /* always 64k pages */
      break;
    }
    default: {
      retval = -NACL_ABI_EINVAL;
      goto cleanup;
    }
  }
  if (!NaClCopyOutToUser(natp->nap, (uintptr_t) result, &result_value,
                         sizeof result_value)) {
    retval = -NACL_ABI_EFAULT;
    goto cleanup;
  }
  retval = 0;
cleanup:
  return retval;
}
