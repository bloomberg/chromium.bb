/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __DUMMY_ERROR_H
#define __DUMMY_ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#undef error
static int error(int status, int errnum, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR(%d): ", errnum);
  vfprintf(stderr, format, ap);
  va_end(ap);
  if (status) exit(status);
  return 0;
}

#endif
