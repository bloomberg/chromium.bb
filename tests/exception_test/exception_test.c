/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include <sys/nacl_syscalls.h>
#include <stdio.h>
#include <stdlib.h>

char stack[4096];

void handler2(int eip, int esp) {
  printf("handler 2 called\n");
  exit(0);
}

void handler(int eip, int esp);

void set_handler2() {
  void (*prev_handler)(int eip, int esp);
  if (0 != NACL_SYSCALL(exception_handler)(handler2, &prev_handler)) {
    printf("failed to set exception handler\n");
    exit(7);
  }
  if (0 != NACL_SYSCALL(exception_stack)((void*)0, 0)) {
    printf("failed to clear exception stack\n");
    exit(8);
  }
  if (prev_handler != handler) {
    printf("failed to get previous exception handler\n");
    exit(3);
  }
}

void handler(int eip, int esp) {
  printf("handler called\n");
  set_handler2();
  if (0 != NACL_SYSCALL(exception_clear_flag)()) {
    printf("failed to clear exception flag\n");
    exit(6);
  }
  *((int*)0) = 0;
  exit(2);
}

void set_handler() {
  if (0 != NACL_SYSCALL(exception_handler)(handler, 0)) {
    printf("failed to set exception handler\n");
    exit(4);
  }
  if (0 != NACL_SYSCALL(exception_stack)(&stack, sizeof(stack))) {
    printf("failed to set alt stack\n");
    exit(5);
  }
}

int main() {
  set_handler();
  /* crash */
  *((int*)0) = 0;
  return 1;
}