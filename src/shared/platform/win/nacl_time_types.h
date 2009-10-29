/*
 * Copyright 2009  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PLATFORM_WIN_NACL_TIME_TYPES_H_
#define NATIVE_CLIENT_SRC_SHARED_PLATFORM_WIN_NACL_TIME_TYPES_H_

#include <windows.h>
#include <sys/timeb.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

/* get NaClMutex declaration, since we need size */
#include "native_client/src/shared/platform/nacl_sync.h"

EXTERN_C_BEGIN

/*
 * NaClTimeState definition for Windows
 */

struct NaClTimeState {
  /*
   * The following are used to set/reset the multimedia timer
   * resolution to the highest possible.  Only used/modified during
   * module initialization/finalization, so can be accessed w/o locks
   * during normal operation.
   */
  UINT      wPeriodMin;
  uint64_t  time_resolution_ns;

  /*
   * The following are used to provide millisecond resolution
   * gettimeofday, and are protected by the mutex lock.
   */
  struct NaClMutex  mu;
  uint64_t          system_time_start_ms;
  DWORD             ms_counter_start;
  uint64_t          epoch_start_ms;
  /*
   * Ensure that reported unix time is monotonically non-decreasing.
   */
  uint64_t          last_reported_time_ms;
};

EXTERN_C_END

#endif
