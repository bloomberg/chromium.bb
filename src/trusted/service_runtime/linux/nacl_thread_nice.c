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
 * Linux thread priority support.
 */
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_nice.h"

static void handle_warning_en(const int en, const char *s) {
  const int kMaxErrorString = 256;
  char errs[kMaxErrorString];
  NaClLog(LOG_WARNING, "%s: %s\n", s, strerror_r(en, errs, sizeof(errs)));
}

void NaClThreadNiceInit() { }

/* Linux version - threads are processes, so use setpriority in lieu
 * of RLIMIT_RTPRIO.
 * This appears to be lockup-free. To enable the privileged realtime
 * threads, /etc/security/limits.conf typically needs to include a
 * line like this:
 *     @audio    -      nice      -10
 * Ubuntu systems (and others?) automatically grant audio group permission
 * to user who login on the console.
 */
/* Linux realtime scheduling is pretty broken at this time. See
 * http://lwn.net/Articles/339316/ for a useful overview.
 */
int nacl_thread_nice(int nacl_nice) {
  const int kRealTimePriority = -10;
  const int kBackgroundPriority = 10;
  const int kNormalPriority = 0;

  switch (nacl_nice) {
    case NICE_REALTIME:
      if (0 == setpriority(PRIO_PROCESS, 0, kRealTimePriority)) {
        return 0;  /* success */
      }
      /* Sorry; no RT priviledges. Fall through to NICE_NORMAL */
    case NICE_NORMAL:
      if (0 == setpriority(PRIO_PROCESS, 0, kNormalPriority)) {
        return 0;  /* success */
      }
      handle_warning_en(errno, "setpriority\n");
      break;
    case NICE_BACKGROUND:
      if (0 == setpriority(PRIO_PROCESS, 0, kBackgroundPriority)) {
        return 0;  /* success */
      }
      handle_warning_en(errno, "setpriority\n");
      break;
    default:
      NaClLog(LOG_WARNING, "nacl_thread_nice failed (bad nice value)\n");
      return -1;
      break;
  }
  return -1;
}
