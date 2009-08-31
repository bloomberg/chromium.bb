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
 * Windows thread priority support.
 */

#include <windows.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_nice.h"

void NaClThreadNiceInit() { }

int nacl_thread_nice(int nacl_nice) {
  BOOL rc;
  HANDLE mThreadHandle = GetCurrentThread();

  switch (nacl_nice) {
    case NICE_REALTIME:
      /* It appears as though you can lock up a machine if you use
       * THREAD_PRIORITY_TIME_CRITICAL or THREAD_PRIORITY_ABOVE_NORMAL.
       * So Windows does not get real-time threads for now.
       */
      rc = SetThreadPriority(mThreadHandle, THREAD_PRIORITY_NORMAL);
                             /* THREAD_PRIORITY_ABOVE_NORMAL); */
                             /* THREAD_PRIORITY_TIME_CRITICAL); */
      break;
    case NICE_NORMAL:
      rc = SetThreadPriority(mThreadHandle, THREAD_PRIORITY_NORMAL);
      break;
    case NICE_BACKGROUND:
      rc = SetThreadPriority(mThreadHandle, THREAD_PRIORITY_BELOW_NORMAL);
      break;
    default:
      NaClLog(LOG_WARNING, "nacl_thread_nice() failed (bad nice value).\n");
      return -1;
      break;
  }
  if (!rc) {
    NaClLog(LOG_WARNING, "nacl_thread_nice() failed.\n");
    return -1;
  }
  return 0;
}
