/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <unistd.h>


int main(void) {
  int pid = getpid();
  assert(pid == -1);
  assert(errno == EACCES);
  return 0;
}
