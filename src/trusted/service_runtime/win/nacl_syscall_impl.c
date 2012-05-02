/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#include <windows.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/timeb.h>

#include <time.h>

#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"
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

#include "native_client/src/trusted/service_runtime/win/nacl_syscall_inl.h"


int32_t NaClSysGetTimeOfDay(struct NaClAppThread      *natp,
                            struct nacl_abi_timeval   *tv,
                            struct nacl_abi_timezone  *tz) {
  int32_t                 retval;
  struct nacl_abi_timeval now;

  UNREFERENCED_PARAMETER(tz);

  NaClLog(3,
          ("Entered NaClSysGetTimeOfDay(%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, (uintptr_t) tv, (uintptr_t) tz);

  /*
   * tz is not supported in linux, nor is it supported by glibc, since
   * tzset(3) and the zoneinfo file should be used instead.
   *
   * TODO(bsy) Do we make the zoneinfo directory available to
   * applications?
   */

  retval = NaClGetTimeOfDay(&now);
  if (0 == retval) {
    CHECK(now.nacl_abi_tv_usec >= 0);
    CHECK(now.nacl_abi_tv_usec < NACL_MICROS_PER_UNIT);
    if (!NaClCopyOutToUser(natp->nap, (uintptr_t) tv, &now, sizeof now)) {
      retval = -NACL_ABI_EFAULT;
    }
  }
  return retval;
}

/*
 * TODO(bsy): REMOVE THIS AND PROVIDE GETRUSAGE.  this is normally not
 * a syscall; instead, it is a library routine on top of getrusage,
 * with appropriate clock tick translation.
 */
/* ARGSUSED */
int32_t NaClSysClock(struct NaClAppThread *natp) {
  int32_t retval;

  NaClLog(3,
          ("Entered NaClSysClock(%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp);

  retval = 1000 * clock();
  /*
   * Windows CLOCKS_PER_SEC is 1000, but XSI requires it to be
   * 1000000L and that's the ABI that we are sticking with.
   *
   * NB: 1000 \cdot n \bmod 2^{32} when n is a 32-bit counter is fine
   * -- user code has to deal with \pmod{2^{32}} wraparound anyway,
   * and time differences will work out fine:
   *
   * \begin{align*}
   * (1000 \cdot \Delta n) \bmod 2^{32}
   *  &\equiv ((1000 \bmod 2^{32}) \cdot (\Delta n \bmod 2^{32}) \bmod 2^{32}\\
   *  &\equiv (1000 \cdot (\Delta n \bmod 2^{32})) \bmod 2^{32}.
   * \end{align*}
   *
   * so when $\Delta n$ is small, the time difference is going to be a
   * small multiple of $1000$, regardless of wraparound.
   */
  return retval;
}

int32_t NaClSysSched_Yield(struct NaClAppThread *natp) {
  SwitchToThread();
  return 0;
}

int32_t NaClSysSysconf(struct NaClAppThread *natp,
                       int32_t              name,
                       int32_t              *result) {
  int32_t         retval = -NACL_ABI_EINVAL;
  static int32_t  number_of_workers = 0;
  int32_t         result_value;

  NaClLog(3,
          ("Entered NaClSysSysconf(%08"NACL_PRIxPTR
           "x, %d, 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, name, (uintptr_t) result);

  switch (name) {
    case NACL_ABI__SC_NPROCESSORS_ONLN: {
      if (0 == number_of_workers) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        number_of_workers = (int32_t) si.dwNumberOfProcessors;
      }
      result_value = number_of_workers;
      break;
    }
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
