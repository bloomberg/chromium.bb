/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for getpwnam/getpwuid/gepwnam_r/getpwuid_r for porting support.
 */

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

struct passwd *getpwnam(const char *name) {
  /* NOTE(sehr): NOT IMPLEMENTED: getpwnam */
  errno = EPERM;
  return NULL;
}

struct passwd *getpwuid(uid_t uid) {
  /* NOTE(sehr): NOT IMPLEMENTED: getpwuid */
  errno = EPERM;
  return NULL;
}

int getpwnam_r(const char *name, struct passwd * pwbuf,
               char *buf, size_t buflen, struct passwd **pwbufp) {
  /* NOTE(sehr): NOT IMPLEMENTED: getpwnam_r */
  return EPERM;
}

int getpwuid_r(uid_t uid, struct passwd * pwbuf,
               char *buf, size_t buflen, struct passwd **pwbufp) {
  /* NOTE(sehr): NOT IMPLEMENTED: getpwuid_r */
  return EPERM;
}
