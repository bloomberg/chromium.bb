/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

/*
 * This guest code is intentionally defined with _start returning an integer
 * in order to test what happens if the sandbox does not exit with a syscall.
 */
int _start(uint32_t info[]) {
  if (info[0] == 1)
    _exit(0xDEADC0DE);
  else
    return 0xDEADFA11;
}
