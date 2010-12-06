/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl expiration check
 */

#include <string.h>
#include <time.h>
#include "native_client/src/trusted/expiration/expiration.h"

/* NOTE: the timefn and mktimefn params will usually be the corresponding
 *  libc functions except for unittesting
 */
int NaClHasExpiredMockable(time_t (*timefn)(time_t *),
                           time_t (*mktimefn)(struct tm *)) {
#if defined(EXPIRATION_CHECK) && defined(NACL_STANDALONE)

  struct tm expiration_tm;
  time_t expiration;
  time_t now;

  if (-1 == timefn(&now)) {
    return 1;
  }

  memset(&expiration_tm, 0, sizeof expiration_tm);
  expiration_tm.tm_mday = EXPIRATION_DAY + 1;
  expiration_tm.tm_mon = EXPIRATION_MONTH - 1;
  expiration_tm.tm_year =  EXPIRATION_YEAR - 1900;
  expiration_tm.tm_isdst = -1;
  expiration = mktimefn(&expiration_tm);
  if (-1 == expiration) {
    return 1;
  }

  return now >= expiration;
#else   /* !defined(EXPIRATION_CHECK) || !defined(NACL_STANDALONE) */
  return 0;
#endif  /* EXPIRATION_CHECK */
}

int NaClHasExpired() {
  return NaClHasExpiredMockable(time, mktime);
}
