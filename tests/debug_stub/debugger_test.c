/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void set_registers_and_stop() {
  /*
   * We set most registers to fixed values before faulting, so that we
   * can test that the debug stub successfully returns the same
   * values.
   *
   * Additionally, we push a value onto the stack in order to test
   * memory accesses.
   */
#if defined(__i386__)
  __asm__("push $0x4bb00ccc\n"
          "mov $0x11000022, %eax\n"
          "mov $0x22000033, %ebx\n"
          "mov $0x33000044, %ecx\n"
          "mov $0x44000055, %edx\n"
          "mov $0x55000066, %esi\n"
          "mov $0x66000077, %edi\n"
          "mov $0x77000088, %ebp\n"
          "hlt\n");
#elif defined(__x86_64__)
  /* Note that we cannot assign arbitrary test values to %r15, %rsp
     and %rbp in the x86-64 sandbox. */
  __asm__("push $0x4bb00ccc\n"
          "mov $0x1100000000000022, %rax\n"
          "mov $0x2200000000000033, %rbx\n"
          "mov $0x3300000000000044, %rcx\n"
          "mov $0x4400000000000055, %rdx\n"
          "mov $0x5500000000000066, %rsi\n"
          "mov $0x6600000000000077, %rdi\n"
          "mov $0x7700000000000088, %r8\n"
          "mov $0x8800000000000099, %r9\n"
          "mov $0x99000000000000aa, %r10\n"
          "mov $0xaa000000000000bb, %r11\n"
          "mov $0xbb000000000000cc, %r12\n"
          "mov $0xcc000000000000dd, %r13\n"
          "mov $0xdd000000000000ee, %r14\n"
          "hlt\n");
#else
# error Update set_registers_and_stop for other architectures
#endif
}

void test_breakpoint() {
#if defined(__i386__) || defined(__x86_64__)
  /*
   * In this bundle int3 (0xcc) is surrounded by instructions that have no
   * 0xcc in their encodings, thus int3 can be located unambiguously.
   */
  __asm__(".align 32\n"
          "nop\n"
          "nop\n"
          "int3\n"
          "nop\n"
          "nop\n");
#else
# error Update test_breakpoint for other architectures
#endif
}

int non_zero_return() {
  return 2;
}

void test_single_step() {
  /*
   * The actual instructions used here don't matter, we just want to use
   * a variety of instruction sizes.
   * Instruction bytes are specified explicitly to avoid ambiguous encodings
   * and thus ensure required instructions sizes.
   */
#if defined(__i386__)
  __asm__(
      ".byte 0xcc\n"                                /* int3 */
      ".byte 0x53\n"                                /* push %ebx */
      ".byte 0x39, 0xd8\n"                          /* cmp  %ebx,%eax */
      ".byte 0x83, 0xeb, 0x01\n"                    /* sub  $0x1,%ebx */
      ".byte 0x81, 0xeb, 0x14, 0x9f, 0x04, 0x08\n"  /* sub  $0x8049f14,%ebx */
      ".byte 0x5b\n");                              /* pop  %ebx */
#elif defined(__x86_64__)
  __asm__(
      ".byte 0xcc\n"                                /* int3 */
      ".byte 0x53\n"                                /* push %rbx */
      ".byte 0x48, 0x39, 0xd8\n"                    /* cmp  %rbx,%rax */
      ".byte 0x48, 0x83, 0xeb, 0x01\n"              /* sub  $0x1,%rbx */
      ".byte 0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00\n"  /* nopw 0x0(%rax,%rax,1) */
      ".byte 0x5b\n");                              /* pop  %rbx */
#else
# error Update test_single_step for other architectures
#endif
}

int main(int argc, char **argv) {
  /*
   * This will crash if the entry-point breakpoint has been mishandled such
   * that our argc and argv values are bogus.  This should catch any
   * regression of http://code.google.com/p/nativeclient/issues/detail?id=1730.
   */
  argv[argc] = 0;

  if (argc < 2) {
    printf("Usage: debugger_test.nexe test_name\n");
    return 1;
  }

  if (strcmp(argv[1], "test_getting_registers") == 0) {
    set_registers_and_stop();
    return 0;
  }
  if (strcmp(argv[1], "test_breakpoint") == 0) {
    test_breakpoint();
    return 0;
  }
  if (strcmp(argv[1], "test_exit_code") == 0) {
    return non_zero_return();
  }
  if (strcmp(argv[1], "test_single_step") == 0) {
    test_single_step();
    return 0;
  }
  return 1;
}
