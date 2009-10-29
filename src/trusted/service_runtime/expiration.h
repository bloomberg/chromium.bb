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
 * NaCl expiration check
 */

#ifndef SERVICE_RUNTIME_EXPIRATION_H__
#define SERVICE_RUNTIME_EXPIRATION_H__  1

#include "native_client/src/include/portability.h"

#include <time.h>

/*
 * NB: expiration semantics is that the expiration day below is the
 * last day on which the function NaClHasExpired will return false
 * (0).
 */
#define EXPIRATION_CHECK 1
#define EXPIRATION_YEAR 2010
#define EXPIRATION_MONTH 1
#define EXPIRATION_DAY 14

EXTERN_C_BEGIN

int NaClHasExpiredMockable(time_t (*timefn)(time_t *),
                           time_t (*mktimefn)(struct tm *));

/*
 * Checks current date against expiration date returns 0 for good,
 * non-zero for expired.  The expiration date is specified in local
 * time, which can be nice in that we'd get rolling expirations,
 * rather than all-at-once global drop dead expiration, but is also
 * somewhat odd.
 */
int NaClHasExpired();

EXTERN_C_END

#endif
