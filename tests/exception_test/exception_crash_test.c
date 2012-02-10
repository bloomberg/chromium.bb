/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


typedef void (*handler_func_t)(int prog_ctr, int stack_ptr);

/*
 * Note that we have to provide an initialiser here otherwise gcc
 * defines this using ".comm" and the variable does not get put into a
 * read-only segment.
 */
const char stack_in_rodata[0x1000] = "blah";


void test_bad_handler() {
  /*
   * Use an address that we know contains no valid code, yet is within
   * the code segment range and is well-aligned.  The bottom 64k of
   * address space is never mapped.
   */
  handler_func_t handler = (handler_func_t) 0x1000;
  int rc = NACL_SYSCALL(exception_handler)(handler, NULL);
  assert(rc == 0);
  fprintf(stderr, "** intended_exit_status=untrusted_segfault\n");
  /* Cause crash. */
  *(volatile int *) 0 = 0;
}

/*
 * TODO(mseaborn): Replace this with an assembly-code handler that
 * switches stack in order to call _exit(1).  This C version will
 * crash if it is run on the bad stacks that we are testing.
 */
void dummy_handler(int prog_ctr, int stack_ptr) {
  /* Should not reach this handler. */
  _exit(1);
}

/*
 * This checks that the process terminates safely if the system
 * attempts to write a stack frame at the current stack pointer when
 * the stack pointer points outside of the sandbox's address space.
 *
 * This only applies to x86-32, because the stack pointer register
 * cannot be set to point outside of the sandbox's address space on
 * x86-64 and ARM.
 */
void test_stack_outside_sandbox() {
#if defined(__i386__)
  int rc = NACL_SYSCALL(exception_handler)(dummy_handler, NULL);
  assert(rc == 0);
  fprintf(stderr, "** intended_exit_status=untrusted_segfault\n");
  asm(/*
       * Set the stack pointer to an address that is definitely
       * outside the sandbox's address space.
       */
      "movl $0xffffffff, %esp\n"
      /* Cause crash. */
      "movl $0, 0\n");
#else
  fprintf(stderr, "test_bad_stack does not apply on this platform\n");
  fprintf(stderr, "** intended_exit_status=0\n");
  exit(0);
#endif
}

void test_stack_in_rodata() {
  int rc = NACL_SYSCALL(exception_handler)(dummy_handler, NULL);
  assert(rc == 0);
  rc = NACL_SYSCALL(exception_stack)((void *) stack_in_rodata,
                                     sizeof(stack_in_rodata));
  assert(rc == 0);
  fprintf(stderr, "** intended_exit_status=unwritable_exception_stack\n");
  /* Cause crash. */
  *(volatile int *) 0 = 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: program <test-name>\n");
    return 1;
  }

#define TRY_TEST(test_name) \
    if (strcmp(argv[1], #test_name) == 0) { test_name(); return 1; }

  TRY_TEST(test_bad_handler);
  TRY_TEST(test_stack_outside_sandbox);
  TRY_TEST(test_stack_in_rodata);

  fprintf(stderr, "Error: Unknown test: \"%s\"\n", argv[1]);
  return 1;
}
