/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * tests whether vaargs, etc. are lowered as expected (ILP32)
 * NOTE: va_start/va_end bracketing is wrong but we do not plan on running
 * this code
 * TODO: alloca is used to create space for the va_list local - why?
 */

#include <stdarg.h>

int foo(char fmt, ...) {
  va_list ap;

  va_start(ap, fmt);

  if (fmt == 'p') {
    return *va_arg(ap, int*);
  }

  if (fmt == 'i') {
    return va_arg(ap, int);
  }

  if (fmt == 'd') {
    return (int) va_arg(ap, double);
  }

  va_end(ap);
  return -1;
}
