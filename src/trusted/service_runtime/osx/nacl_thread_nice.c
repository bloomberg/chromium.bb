/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Mac OSX thread priority support.
 */

#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_nice.h"

/* MyConvertToHostTime converts nanoseconds to the time base used by MacOS.
 * Typically the time base is nanoseconds, and the conversion is trivial.
 * To avoid having to drag in the entire CoreAudio framework just for this
 * subroutine, it is reimplemented here in terms of mach_timebase_info which
 * is in the MacOS core.
 * To understand that this implementation is correct, have a look at
 * CAHostTimeBase::ConvertFromNanos(uint64 inNanos) in CAHostTimeBase.h
 */
static struct mach_timebase_info gTimeBase;
void NaClThreadNiceInit() {
  if (mach_timebase_info(&gTimeBase) != KERN_SUCCESS) {
    NaClLog(LOG_WARNING, "mach_timebase_info failed\n");
    gTimeBase.numer = 1;
    gTimeBase.denom = 1;
  }
  if (gTimeBase.denom == 0) {
    NaClLog(LOG_WARNING, "mach_timebase_info returned bogus result\n");
    gTimeBase.numer = 1;
    gTimeBase.denom = 1;
  }
}

static uint64_t MyConvertToHostTime(uint64_t nanos) {
  double math_tmp = 0.0;

  math_tmp = gTimeBase.numer;
  math_tmp *= nanos;
  math_tmp /= gTimeBase.denom;
  return (uint64_t)math_tmp;
}

int nacl_thread_nice(int nacl_nice) {
  kern_return_t kr;
  thread_act_t mthread = mach_thread_self();

  switch (nacl_nice) {
    case NICE_REALTIME: {
      struct thread_time_constraint_policy tcpolicy;
      const int kPeriodInNanoseconds = 2902490;
      const float kDutyCycle = 0.5;
      const float kDutyMax = 0.85;
      tcpolicy.period = MyConvertToHostTime(kPeriodInNanoseconds);
      tcpolicy.computation = kDutyCycle * tcpolicy.period;
      tcpolicy.constraint = kDutyMax * tcpolicy.period;
      tcpolicy.preemptible = 1;
      /* Sadly it appears that a MacOS system can be locked up by too
       * many real-time threads. So use normal priority until we figure
       * out a way to control things globally.
       */
      /* kr = thread_policy_set(mthread, THREAD_TIME_CONSTRAINT_POLICY,
       *                        (thread_policy_t)&tcpolicy,
       *                        THREAD_TIME_CONSTRAINT_POLICY_COUNT);
       */
      kr = thread_policy_set(mthread, THREAD_PRECEDENCE_POLICY,
                             (thread_policy_t)&tcpolicy,
                             THREAD_PRECEDENCE_POLICY_COUNT);
    }
      break;
    case NICE_BACKGROUND: {
      struct thread_precedence_policy tppolicy;
      tppolicy.importance = 0;  /* IDLE_PRI */
      kr = thread_policy_set(mthread, THREAD_PRECEDENCE_POLICY,
                             (thread_policy_t)&tppolicy,
                             THREAD_PRECEDENCE_POLICY_COUNT);
    }
      break;
    case NICE_NORMAL: {
      struct thread_standard_policy tspolicy;
      kr = thread_policy_set(mthread, THREAD_STANDARD_POLICY,
                             (thread_policy_t)&tspolicy,
                             THREAD_STANDARD_POLICY_COUNT);
    }
      break;
    default:
      NaClLog(LOG_WARNING, "nacl_thread_nice() failed (bad nice value).\n");
      return -1;
      break;
  }
  if (kr != KERN_SUCCESS) {
    NaClLog(LOG_WARNING, "nacl_thread_nice() failed.\n");
    return -1;
  } else {
    return 0;
  }
}
