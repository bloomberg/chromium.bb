/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for getcwd/get_current_dir_name/getwd for porting support.
 */

#include <unistd.h>
#include <errno.h>

char *getcwd(char *buf, size_t size) {
  /* NOTE(sehr): NOT IMPLEMENTED: getcwd */
  errno = EACCES;
  return NULL;
}

char *get_current_dir_name(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: get_current_dir_name */
  errno = EACCES;
  return NULL;
}

char *getwd(char *buf) {
  /* NOTE(sehr): NOT IMPLEMENTED: getwd */
  errno = EACCES;
  return NULL;
}
