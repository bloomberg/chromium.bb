/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <unistd.h>

void _start(uint32_t info[]) {
  _exit(0xCAFEBABE);
}
