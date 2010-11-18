/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
#ifdef NACL_STANDALONE  /* NaCl in Chrome does not expire */
#define EXPIRATION_CHECK 1
#define EXPIRATION_YEAR 2011
#define EXPIRATION_MONTH 2
#define EXPIRATION_DAY 17
#endif

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
