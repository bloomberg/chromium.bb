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

/*
 * These variables are set by the assembly code.  We initialise them
 * to dummy values to avoid false positives in case the assembly code
 * does not get run.
 */
#if defined(__i386__)
uint32_t exception_handler_esp = -1;
#elif defined(__x86_64__)
uint64_t exception_handler_rsp = -1;
uint64_t exception_handler_rbp = -1;
#endif
void exception_handler_wrapper(int prog_ctr, int stack_ptr);

char stack[4096];

jmp_buf g_jmp_buf;

char *g_registered_stack;
size_t g_registered_stack_size;

char *main_stack;


#define STACK_ALIGNMENT 16

/*
 * The x86-64 ABI's red zone is 128 bytes of scratch space below %rsp
 * which a function may use without it being clobbered by a signal
 * handler.
 *
 * Note that if we are running the pure-bitcode version of the test on
 * x86-64, the exact value of kRedZoneSize does not matter.
 */
#if defined(__x86_64__)
const int kRedZoneSize = 128;
#else
const int kRedZoneSize = 0;
#endif

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


void exception_handler(int eip, int esp) {
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

  char *stack_top;
  char local_var;
  if (g_registered_stack == NULL) {
    /* Check that our current stack is just below the saved stack pointer. */
    stack_top = (char *) esp - kRedZoneSize;
    assert(stack_top - kMaxStackFrameSize < &local_var);
    assert(&local_var < stack_top);
  } else {
    /* Check that we are running on the exception stack. */
    stack_top = g_registered_stack + g_registered_stack_size;
    assert(g_registered_stack <= &local_var);
    assert(&local_var < stack_top);
  }

  /*
   * If we can link in assembly code, we can check the exception
   * handler's initial stack pointer more exactly.
   */
#if defined(__i386__)
  /* Skip over the 4 byte return address. */
  uintptr_t frame_base = exception_handler_esp + 4;
  assert(frame_base % STACK_ALIGNMENT == 0);
  /* Skip over the 2 arguments, which are 4 bytes each. */
  char *frame_top = (char *) (frame_base + 2 * 4);
  /* Check that no more than the stack alignment size is wasted. */
  assert(stack_top - STACK_ALIGNMENT < frame_top);
  assert(frame_top <= stack_top);
#elif defined(__x86_64__)
  /* Skip over the 8 byte return address. */
  uintptr_t frame_base = ((uint32_t) exception_handler_rsp) + 8;
  assert(frame_base % STACK_ALIGNMENT == 0);
  /* Nothing else is pushed onto the stack yet. */
  char *frame_top = (char *) frame_base;
  /* Check that no more than the stack alignment size is wasted. */
  assert(stack_top - STACK_ALIGNMENT < frame_top);
  assert(frame_top <= stack_top);

  /* Check that %rbp has been reset to a safe value. */
  uint64_t sandbox_base;
  asm("mov %%r15, %0" : "=m"(sandbox_base));
  assert(exception_handler_rbp == sandbox_base);
  assert(exception_handler_rsp >> 32 == sandbox_base >> 32);
#endif

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
  handler_func_t handler;
#ifndef __pnacl__
  handler = exception_handler_wrapper;
#else
  handler = exception_handler;
#endif

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

  rc = NACL_SYSCALL(exception_handler)(exception_handler, NULL);
  assert(rc == 0);

  rc = NACL_SYSCALL(exception_handler)(NULL, &prev_handler);
  assert(rc == 0);
  assert(prev_handler == exception_handler);

  rc = NACL_SYSCALL(exception_handler)(NULL, &prev_handler);
  assert(rc == 0);
  assert(prev_handler == NULL);
}

void test_invalid_handlers() {
  int rc;
  handler_func_t unaligned_func_ptr =
    (handler_func_t) ((uintptr_t) exception_handler + 1);
  const char *ptr_in_rodata_segment = "";

  /* An alignment check is required for safety in all NaCl sandboxes. */
  rc = NACL_SYSCALL(exception_handler)(unaligned_func_ptr, NULL);
  assert(rc == -EFAULT);

  /* A range check is required for safety in the NaCl ARM sandbox. */
  rc = NACL_SYSCALL(exception_handler)(
      (handler_func_t) (uintptr_t) ptr_in_rodata_segment, NULL);
  assert(rc == -EFAULT);
}


#if defined(__i386__) || defined(__x86_64__)

int get_x86_direction_flag(void);

void test_get_x86_direction_flag() {
  /*
   * Sanity check:  Ensure that get_x86_direction_flag() works.  We
   * avoid calling assert() with the flag set, because that might not
   * work.
   */
  assert(get_x86_direction_flag() == 0);
  asm("std");
  int flag = get_x86_direction_flag();
  asm("cld");
  assert(flag == 1);
}

void direction_flag_exception_handler(int prog_ctr, int stack_ptr) {
  assert(get_x86_direction_flag() == 0);
  /* Return from exception handler. */
  int rc = NACL_SYSCALL(exception_clear_flag)();
  assert(rc == 0);
  longjmp(g_jmp_buf, 1);
}

/*
 * The x86 ABIs require that the x86 direction flag is unset on entry
 * to a function.  However, a crash could occur while the direction
 * flag is set.  In order for an exception handler to be a normal
 * function without x86-specific knowledge, NaCl needs to unset the
 * direction flag when calling the exception handler.  This test
 * checks that this happens.
 */
void test_unsetting_x86_direction_flag() {
  printf("test_unsetting_x86_direction_flag...\n");
  int rc = NACL_SYSCALL(exception_handler)(direction_flag_exception_handler,
                                           NULL);
  assert(rc == 0);
  if (!setjmp(g_jmp_buf)) {
    /* Cause a crash with the direction flag set. */
    asm("std");
    *((volatile int *) 0) = 0;
    /* Should not reach here. */
    exit(1);
  }
  /* Clear the jmp_buf to prevent it from being reused accidentally. */
  memset(g_jmp_buf, 0, sizeof(g_jmp_buf));
}

#endif


int main() {
  /* Test exceptions without having an exception stack set up. */
  test_exception_stack_with_size(NULL, 0);

  test_exception_stack_with_size(stack, sizeof(stack));

  /*
   * Test the stack realignment logic by trying stacks which end at
   * different addresses modulo the stack alignment size.
   */
  int diff;
  for (diff = 0; diff <= STACK_ALIGNMENT * 2; diff++) {
    test_exception_stack_with_size(stack, sizeof(stack) - diff);
  }

  test_getting_previous_handler();
  test_invalid_handlers();

#if defined(__i386__) || defined(__x86_64__)
  test_get_x86_direction_flag();
  test_unsetting_x86_direction_flag();
#endif

  fprintf(stderr, "** intended_exit_status=0\n");
  return 0;
}
