/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for getpwent/setpwent/endpwent for porting support.
 */

#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

struct passwd *getpwent(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: getpwent */
  errno = ENOMEM;
  return NULL;
}

void setpwent(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: setpwent */
}

void endpwent(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: endpwent */
}
