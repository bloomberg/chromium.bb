/*
 * Copyright 2008  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime time abstraction layer.
 * This is the host-OS-dependent implementation.
 */

#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#include <time.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/platform/win/nacl_time_types.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

/*
 * To get a 1ms resolution time-of-day clock, we have to jump thrugh
 * some hoops.  This approach has the following defect: ntp
 * adjustments will be nullified, until the difference is larger than
 * a recalibration threshold (below), at which point time may jump
 * (until we introduce a mechanism to slowly phase in time changes).
 * A system suspend/resume will generate a clock recalibration with
 * high probability.
 *
 * The algorithm is as follows:
 *
 * At start up, we sample the system time (coarse timer) via
 * GetSystemTimeAsFileTime until it changes.  At the state transition,
 * we record the value of the time-after-boot counter via timeGetTime.
 *
 * At subsequent gettimeofday syscall, we query the system time via
 * GetSystemTimeAsFileTime.  This is converted to an
 * expected/corresponding return value from timeGetTime.  The
 * difference from the actual return value is the time that elapsed
 * since the state transition of the system time counter, and we add
 * that difference (assuming it is smaller than the max-drift
 * threshold) to the system time and return that (minus the Unix
 * epoch, etc), as the result of the gettimeofday syscall.  If the
 * difference is larger than the max-drift threshold, we invoke the
 * calibration function.
 */

static struct NaClTimeState gNaClTimeState;

static uint32_t const kMaxMillsecondDriftBeforeRecalibration = 60;
static uint32_t const kMaxCalibrationDiff = 4;

/*
 * Translate Windows system time to milliseconds.  Windows System time
 * is in units of 100nS.  Windows time is in 100e-9s == 1e-7s, and we
 * want units of 1e-3s, so ms/wt = 1e-4.
 */
static uint64_t NaClFileTimeToMs(FILETIME *ftp) {
  ULARGE_INTEGER t;

  t.u.HighPart = ftp->dwHighDateTime;
  t.u.LowPart = ftp->dwLowDateTime;
  return t.QuadPart / NACL_100_NANOS_PER_MILLI;
}

/*
 * Calibration is done as described above.
 *
 * To ensure that the calibration is accurate, we interleave sampling
 * the millisecond resolution counter via timeGetTime with the 10-55
 * millisecond resolution system clock via GetSystemTimeAsFileTime.
 * When we see the edge transition where the system clock ticks, we
 * compare the before and after millisecond counter values.  If it is
 * within a calibration threshold (kMaxCalibrationDiff), then we
 * record the instantaneous system time (10-55 msresolution) and the
 * 32-bit counter value (1ms reslution).  Since we have a before and
 * after counter value, we interpolate it to get the "average" counter
 * value to associate with the system time.
 */
static void NaClCalibrateWindowsClockMu(struct NaClTimeState *ntsp) {
  FILETIME  ft_start;
  FILETIME  ft_now;
  DWORD     ms_counter_before;
  DWORD     ms_counter_after;
  uint32_t  ms_counter_diff;

  NaClLog(4, "Entered NaClCalibrateWindowsClockMu\n");
  GetSystemTimeAsFileTime(&ft_start);
  ms_counter_before = timeGetTime();
  for (;;) {
    GetSystemTimeAsFileTime(&ft_now);
    ms_counter_after = timeGetTime();
    ms_counter_diff = ms_counter_after - (uint32_t) ms_counter_before;
    NaClLog(4, "ms_counter_diff %u\n", ms_counter_diff);
    if (ms_counter_diff <= kMaxCalibrationDiff &&
        (ft_now.dwHighDateTime != ft_start.dwHighDateTime ||
         ft_now.dwLowDateTime != ft_start.dwLowDateTime)) {
      break;
    }
    ms_counter_before = ms_counter_after;
  }
  ntsp->system_time_start_ms = NaClFileTimeToMs(&ft_now);
  /*
   * Average the counter values.  Note unsigned computation of
   * ms_counter_diff, so that was mod 2**32 arithmetic, and the
   * addition of half the difference is numerically correct, whereas
   * (ms_counter_before + ms_counter_after)/2 is wrong due to
   * overflow.
   */
  ntsp->ms_counter_start = (DWORD) (ms_counter_before + (ms_counter_diff / 2));

  NaClLog(4, "Leaving NaClCalibrateWindowsClockMu\n");
}

void NaClTimeInternalInit(struct NaClTimeState *ntsp) {
  TIMECAPS    tc;
  SYSTEMTIME  st;
  FILETIME    ft;

  /*
   * Maximize timer/Sleep resolution.
   */
  timeGetDevCaps(&tc, sizeof tc);
  ntsp->wPeriodMin = tc.wPeriodMin;
  ntsp->time_resolution_ns = tc.wPeriodMin * NACL_NANOS_PER_MILLI;
  NaClLog(0, "NaClTimeInternalInit: timeBeginPeriod(%u)\n", tc.wPeriodMin);
  timeBeginPeriod(ntsp->wPeriodMin);

  /*
   * Compute Unix epoch start; calibrate high resolution clock.
   */
  st.wYear = 1970;
  st.wMonth = 1;
  st.wDay = 1;
  st.wHour = 0;
  st.wMinute = 0;
  st.wSecond = 0;
  st.wMilliseconds = 0;
  SystemTimeToFileTime(&st, &ft);
  ntsp->epoch_start_ms = NaClFileTimeToMs(&ft);
  ntsp->last_reported_time_ms = 0;
  NaClLog(0, "Unix epoch start is  %"NACL_PRIu64"ms in Windows epoch time\n",
          ntsp->epoch_start_ms);
  NaClMutexCtor(&ntsp->mu);
  /*
   * We don't actually grab the lock, since the module initializer
   * should be called before going threaded.
   */
  NaClCalibrateWindowsClockMu(ntsp);
}

uint64_t NaClTimerResolutionNsInternal(struct NaClTimeState *ntsp) {
  return ntsp->time_resolution_ns;
}

void NaClTimeInternalFini(struct NaClTimeState *ntsp) {
  NaClMutexDtor(&ntsp->mu);
  timeEndPeriod(ntsp->wPeriodMin);
}

void NaClTimeInit(void) {
  NaClTimeInternalInit(&gNaClTimeState);
}

void NaClTimeFini(void) {
  NaClTimeInternalInit(&gNaClTimeState);
}

uint64_t NaClTimerResolutionNanoseconds(void) {
  return NaClTimerResolutionNsInternal(&gNaClTimeState);
}

int NaClGetTimeOfDayIntern(struct nacl_abi_timeval *tv,
                           struct NaClTimeState    *ntsp) {
  FILETIME  ft_now;
  DWORD     ms_counter_now;
  uint64_t  t_ms;
  DWORD     ms_counter_at_ft_now;
  uint32_t  ms_counter_diff;
  uint64_t  unix_time_ms;

  GetSystemTimeAsFileTime(&ft_now);
  ms_counter_now = timeGetTime();
  t_ms = NaClFileTimeToMs(&ft_now);

  NaClMutexLock(&ntsp->mu);

  NaClLog(5, "ms_counter_now       %"NACL_PRIu32"\n",
          (uint32_t) ms_counter_now);
  NaClLog(5, "t_ms                 %"NACL_PRId64"\n", t_ms);
  NaClLog(5, "system_time_start_ms %"NACL_PRIu64"\n",
          ntsp->system_time_start_ms);

  ms_counter_at_ft_now = (DWORD)
      (ntsp->ms_counter_start +
       (uint32_t) (t_ms - ntsp->system_time_start_ms));

  NaClLog(5, "ms_counter_at_ft_now %"NACL_PRIu32"\n",
          (uint32_t) ms_counter_at_ft_now);

  ms_counter_diff = ms_counter_now - (uint32_t) ms_counter_at_ft_now;

  NaClLog(5, "ms_counter_diff      %"NACL_PRIu32"\n", ms_counter_diff);

  if (ms_counter_diff <= kMaxMillsecondDriftBeforeRecalibration) {
    t_ms = t_ms + ms_counter_diff;
  } else {
    NaClCalibrateWindowsClockMu(ntsp);
    t_ms = ntsp->system_time_start_ms;
  }

  NaClLog(5, "adjusted t_ms =      %"NACL_PRIu64"\n", t_ms);

  unix_time_ms = t_ms - ntsp->epoch_start_ms;

  /*
   * Time is monotonically non-decreasing.
   */
  if (unix_time_ms < ntsp->last_reported_time_ms) {
    unix_time_ms = ntsp->last_reported_time_ms;
  } else {
    ntsp->last_reported_time_ms = unix_time_ms;
  }

  NaClMutexUnlock(&ntsp->mu);

  NaClLog(5, "unix_time_ms  =      %"NACL_PRId64"\n", unix_time_ms);
  /*
   * Unix time is measured relative to a different epoch, Jan 1, 1970.
   * See the module initialization for epoch_start_ms.
   */

  tv->nacl_abi_tv_sec = (nacl_abi_time_t) (unix_time_ms / 1000);
  tv->nacl_abi_tv_usec = (nacl_abi_suseconds_t) ((unix_time_ms % 1000) * 1000);

  NaClLog(5, "nacl_avi_tv_sec =    %"NACL_PRIdNACL_TIME"\n",
          tv->nacl_abi_tv_sec);
  NaClLog(5, "nacl_avi_tv_usec =   %"NACL_PRId32"\n", tv->nacl_abi_tv_usec);

  return 0;
}

int NaClGetTimeOfDay(struct nacl_abi_timeval *tv) {
  return NaClGetTimeOfDayIntern(tv, &gNaClTimeState);
}

int NaClNanosleep(struct nacl_abi_timespec const *req,
                  struct nacl_abi_timespec       *rem) {
  DWORD                     sleep_ms;
  uint64_t                  resolution;
  DWORD                     resolution_gap = 0;

  UNREFERENCED_PARAMETER(rem);

  /* round up from ns resolution to ms resolution */
  sleep_ms = (req->tv_sec * NACL_MILLIS_PER_UNIT +
              NACL_UNIT_CONVERT_ROUND(req->tv_nsec, NACL_NANOS_PER_MILLI));

  /* round up to minimum timer resolution */
  resolution = NaClTimerResolutionNanoseconds();
  NaClLog(4, "Resolution %"NACL_PRId64"\n", resolution);
  if (0 != resolution) {
    resolution = NACL_UNIT_CONVERT_ROUND(resolution, NACL_NANOS_PER_MILLI);
    resolution_gap = (DWORD) (sleep_ms % resolution);
    if (0 != resolution_gap) {
      resolution_gap = (DWORD) (resolution - resolution_gap);
    }
  }
  NaClLog(4, "Resolution gap %d\n", resolution_gap);
  sleep_ms += resolution_gap;

  NaClLog(4, "Sleep(%d)\n", sleep_ms);
  Sleep(sleep_ms);

  return 0;
}
