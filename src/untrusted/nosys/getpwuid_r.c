/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `getpwuid_r' for porting support.
 */

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

int getpwuid_r(uid_t uid, struct passwd * pwbuf,
               char *buf, size_t buflen, struct passwd **pwbufp) {
  return ENOSYS;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(getpwuid_r);
