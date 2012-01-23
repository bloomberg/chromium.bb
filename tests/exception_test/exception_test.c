/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


#ifndef __pnacl__
void crash_at_known_address(void);
extern uintptr_t stack_ptr_at_crash;
extern char prog_ctr_at_crash[];
#endif

char stack[4096];

char *main_stack;


#define STACK_ALIGNMENT 32

struct AlignedType {
  int blah;
} __attribute__((aligned(16)));

/*
 * We do this check in a separate function in an attempt to prevent
 * the compiler from optimising away the check for a stack-allocated
 * variable.
 *
 * We test for an alignment that is small enough for the compiler to
 * assume on x86-32, even if sel_ldr sets up a larger alignment.
 */
__attribute__((noinline))
void check_pointer_is_aligned(void *pointer) {
  assert((uintptr_t) pointer % 16 == 0);
}

void check_stack_is_aligned() {
  struct AlignedType var;
  check_pointer_is_aligned(&var);
}


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

  check_stack_is_aligned();

  /* Check that we are running on the exception stack. */
  char *stack_top = stack + sizeof(stack);
  char local_var;
  assert(stack <= &local_var && &local_var < stack_top);
  /*
   * On x86-32, we can check our stack more exactly because arguments
   * are passed on the stack, and the last argument should be at the
   * top of the exception stack.
   */
#ifdef __i386__
  char *frame_top = (char *) (&esp + 1);
  /* Check that no more than the stack alignment size is wasted. */
  assert(stack_top - STACK_ALIGNMENT < frame_top);
  assert(frame_top <= stack_top);
#endif

  /*
   * Check the saved stack pointer.  We cannot portably test this, but
   * we assume the faulting function's stack frame is not excessively
   * large.  We assume the stack grows down.
   */
  assert(main_stack - 0x1000 < (char *) esp && (char *) esp < main_stack);
  /*
   * If we can link in assembly code, we can check the saved values
   * more exactly.
   */
#ifndef __pnacl__
  assert(esp == stack_ptr_at_crash);
  assert(eip == (uintptr_t) prog_ctr_at_crash);
#endif

  set_handler2();
  if (0 != NACL_SYSCALL(exception_clear_flag)()) {
    printf("failed to clear exception flag\n");
    exit(6);
  }
  *((volatile int *) 0) = 0;
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

  /* Record the address of the current stack for testing against later. */
  char local_var;
  main_stack = &local_var;

  /*
   * We need a memory barrier if we are using a volatile assignment to
   * cause the crash, because otherwise the compiler might reorder the
   * assignment of "main_stack" to after the volatile assignment.
   */
  __sync_synchronize();
#ifdef __pnacl__
  /* Cause crash. */
  *((volatile int *) 0) = 0;
#else
  crash_at_known_address();
#endif

  return 1;
}
