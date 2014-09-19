/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <limits.h>
#include <string.h>

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/minsfi_syscalls.h"
#include "native_client/src/include/nacl_assert.h"

/*
 * This tests the untrusted implementation of sbrk() which uses the 'heaplim'
 * MinSFI syscall. We check that the program break (see sbrk() man) advances
 * correctly and that it fails once it reaches the upper limit of the heap.
 *
 * The sandbox will invoke sbrk() with an increment value passed to it, and
 * then run and return sbrk(0) to obtain the new program break value.
 */
void test_sbrk(void) {
  const MinsfiSandbox *sb;
  uint32_t ret_val, inc;
  char buf[16];
  char *argv_sbrk[2] = { "sbrk", NULL };

  /* Initialize and verify that we got the size of the heap right. */
  MinsfiInitializeSandbox();
  sb = MinsfiGetActiveSandbox();

  /* Read the initial program break value. Should be the base of the heap. */
  argv_sbrk[1] = "0";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset);

  /* Increment by 19 */
  argv_sbrk[1] = "19";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset + 19);

  /* Decrement by 19 */
  argv_sbrk[1] = "-19";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset);

  /*
   * Check that attempts to decrement further will fail, and that the program
   * break remains unchanged.
   */
  argv_sbrk[1] = "-1";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ((int) ret_val, -1);
  argv_sbrk[1] = "0";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset);

  /* Increment by the maximum possible amount. We might have to do this in
   * two steps if the increment value does not fit into a signed integer. */
  argv_sbrk[1] = buf;
  inc = sb->mem_layout.heap.length;
  if (inc > INT_MAX) {
    inc -= INT_MAX;
    sprintf(buf, "%d", INT_MAX);
    ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
    ASSERT_EQ(ret_val, sb->mem_layout.heap.offset + INT_MAX);
  }
  sprintf(buf, "%d", inc);
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset + sb->mem_layout.heap.length);

  /* Now sbrk should not be able to increase the program break any more. */
  argv_sbrk[1] = "1";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ((int) ret_val, -1);
  argv_sbrk[1] = "0";
  ret_val = MinsfiInvokeSandbox(2, argv_sbrk);
  ASSERT_EQ(ret_val, sb->mem_layout.heap.offset + sb->mem_layout.heap.length);

  /* Clean up. */
  MinsfiDestroySandbox();
}

/*
 * We don't do any comprehensive testing of malloc (it runs sandboxed anyway).
 * We only verify that it correctly determines the page size and uses sbrk()
 * to allocate more memory for the heap.
 */
void test_malloc(void) {
  int malloc_ret, brk_initial, brk_after, i, pagesize, inc;
  char *argv_sbrk0[2] = { "sbrk", "0" };
  char arg_inc[16];
  char *argv_malloc[2] = { "malloc", arg_inc };

  /* Prepare the sandbox. */
  MinsfiInitializeSandbox();

  /*
   * Compute the size of the block we will try to allocate. We take one fourth
   * of the page size plus one, and then invoke malloc twenty times. That
   * should force malloc to allocate six new pages with sbrk().
   */
  pagesize = sysconf(_SC_PAGESIZE);
  ASSERT_NE(-1, pagesize);
  inc = (pagesize / 4) + 1;
  sprintf(arg_inc, "%d\n", inc);

  /* Keep allocating memory and observe how program break changes. */
  brk_initial = MinsfiInvokeSandbox(2, argv_sbrk0);
  for (i = 0; i < 20; i++) {
    malloc_ret = MinsfiInvokeSandbox(2, argv_malloc);
    brk_after = MinsfiInvokeSandbox(2, argv_sbrk0);

    /* Check that the malloc-ed memory is on the untrusted heap. */
    ASSERT_GE(malloc_ret, brk_initial);
    ASSERT_LE(malloc_ret + inc, brk_after);

    /* Check that the program break remains page-aligned. */
    ASSERT(!(brk_after & (pagesize - 1)));
  }
  ASSERT_EQ(brk_initial + 6 * pagesize, brk_after);

  /* Clean up. */
  MinsfiDestroySandbox();
}

int main(void) {
  test_sbrk();
  test_malloc();
  return 0;
}
