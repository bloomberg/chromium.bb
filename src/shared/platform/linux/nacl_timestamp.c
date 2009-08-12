/*
 * Copyright 2008, Google Inc.
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
 * NaCl Service Runtime logging code.
 */

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "native_client/src/shared/platform/nacl_timestamp.h"

char  *NaClTimeStampString(char   *buffer,
                           size_t buffer_size) {
  struct timeval  tv;
  struct tm       bdt;  /* broken down time */

  if (-1 == gettimeofday(&tv, (struct timezone *) NULL)) {
    snprintf(buffer, buffer_size, "-NA-");
    return buffer;
  }
  (void) localtime_r(&tv.tv_sec, &bdt);
  /* suseconds_t holds at most 10**6 < 16**6 = (2**4)**6 = 2**24 < 2**32 */
  snprintf(buffer, buffer_size, "%02d:%02d:%02d.%06d",
           bdt.tm_hour, bdt.tm_min, bdt.tm_sec, (int32_t) tv.tv_usec);
  return buffer;
}
