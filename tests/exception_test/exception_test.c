/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


typedef void (*handler_func_t)(int prog_ctr, int stack_ptr);

#ifndef __pnacl__
void crash_at_known_address(void);
extern uintptr_t stack_ptr_at_crash;
extern char prog_ctr_at_crash[];
#endif

char stack[4096];

jmp_buf g_jmp_buf;

char *g_registered_stack;
size_t g_registered_stack_size;

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


void handler(int eip, int esp) {
  printf("handler called\n");

  check_stack_is_aligned();

  /*
   * Check the saved stack pointer.  We cannot portably test this, but
   * we assume the faulting function's stack frame is not excessively
   * large.  We assume the stack grows down.
   */
  const int kMaxStackFrameSize = 0x1000;
  assert(main_stack - kMaxStackFrameSize < (char *) esp);
  assert((char *) esp < main_stack);
  /*
   * If we can link in assembly code, we can check the saved values
   * more exactly.
   */
#ifndef __pnacl__
  assert(esp == stack_ptr_at_crash);
  assert(eip == (uintptr_t) prog_ctr_at_crash);
#endif

  char local_var;
  if (g_registered_stack == NULL) {
    assert((char *) esp - kMaxStackFrameSize < &local_var);
    assert(&local_var < (char *) esp);
  } else {
    /* Check that we are running on the exception stack. */
    char *stack_top = g_registered_stack + g_registered_stack_size;
    assert(g_registered_stack <= &local_var);
    assert(&local_var < stack_top);
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
  }

  /*
   * Clear the exception flag so that future faults will invoke the
   * exception handler.
   */
  if (0 != NACL_SYSCALL(exception_clear_flag)()) {
    printf("failed to clear exception flag\n");
    exit(6);
  }
  longjmp(g_jmp_buf, 1);
}

void test_exception_stack_with_size(char *stack, size_t stack_size) {
  if (0 != NACL_SYSCALL(exception_handler)(handler, 0)) {
    printf("failed to set exception handler\n");
    exit(4);
  }
  if (0 != NACL_SYSCALL(exception_stack)(stack, stack_size)) {
    printf("failed to set alt stack\n");
    exit(5);
  }
  g_registered_stack = stack;
  g_registered_stack_size = stack_size;

  /* Record the address of the current stack for testing against later. */
  char local_var;
  main_stack = &local_var;

  if (!setjmp(g_jmp_buf)) {
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
    /* Should not reach here. */
    exit(1);
  }
  /* Clear the jmp_buf to prevent it from being reused accidentally. */
  memset(g_jmp_buf, 0, sizeof(g_jmp_buf));
}

void test_getting_previous_handler() {
  int rc;
  handler_func_t prev_handler;

  rc = NACL_SYSCALL(exception_handler)(handler, NULL);
  assert(rc == 0);

  rc = NACL_SYSCALL(exception_handler)(NULL, &prev_handler);
  assert(rc == 0);
  assert(prev_handler == handler);

  rc = NACL_SYSCALL(exception_handler)(NULL, &prev_handler);
  assert(rc == 0);
  assert(prev_handler == NULL);
}

void test_invalid_handlers() {
  int rc;
  handler_func_t unaligned_func_ptr =
    (handler_func_t) ((uintptr_t) handler + 1);
  const char *ptr_in_rodata_segment = "";

  /* An alignment check is required for safety in all NaCl sandboxes. */
  rc = NACL_SYSCALL(exception_handler)(unaligned_func_ptr, NULL);
  assert(rc == -EFAULT);

  /* A range check is required for safety in the NaCl ARM sandbox. */
  rc = NACL_SYSCALL(exception_handler)(
      (handler_func_t) (uintptr_t) ptr_in_rodata_segment, NULL);
  assert(rc == -EFAULT);
}

int main() {
  /* Test exceptions without having an exception stack set up. */
  test_exception_stack_with_size(NULL, 0);

  test_exception_stack_with_size(stack, sizeof(stack));

  /*
   * Test the stack realignment logic by trying stacks which end at
   * different addresses modulo the stack alignment size.
   */
  int diff;
  for (diff = 0; diff <= STACK_ALIGNMENT; diff++) {
    test_exception_stack_with_size(stack, sizeof(stack) - diff);
  }

  test_getting_previous_handler();
  test_invalid_handlers();

  fprintf(stderr, "** intended_exit_status=0\n");
  return 0;
}
