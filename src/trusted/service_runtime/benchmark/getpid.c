/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/syscall.h>
#include <unistd.h>

int num_syscalls = 100000000;


int my_getpid(void) {
  return syscall(SYS_getpid);
}


int main(void) {
  int p;
  int i;

  for (i = p = 0; i < num_syscalls; ++i) {
    p += my_getpid();
  }
  return p;
}
