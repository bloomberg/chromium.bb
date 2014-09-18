/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/nacl_assert.h"

static int argc_exit_with_syscall = 1;
static int argc_exit_with_return = 2;
static char *argv[] = { "foo", "bar" };

/*
 * In this test, the sandboxed code invokes the exit() MinSFI syscall.
 * We verify that it jumps back to the Invoke function and that it returns
 * the same status code that the sandbox terminated with.
 */
static void test_exit_with_syscall(void) {
  int ret_val;

  MinsfiInitializeSandbox();
  ret_val = MinsfiInvokeSandbox(argc_exit_with_syscall, argv);
  ASSERT_EQ(ret_val, (int) 0xDEADC0DE);
  MinsfiDestroySandbox();
}

/*
 * In this test, the sandbox does not invoke the exit() syscall, but rather
 * returns the status code 0xDEADFA11 from the entry function. That is not
 * a legal way of terminating the sandbox and the Invoke function should report
 * failure.
 */
static void test_exit_with_return(void) {
  int ret_val;

  MinsfiInitializeSandbox();
  ret_val = MinsfiInvokeSandbox(argc_exit_with_return, argv);
  ASSERT_EQ(ret_val, EXIT_FAILURE);
  MinsfiDestroySandbox();
}

int main(void) {
  test_exit_with_return();
  test_exit_with_syscall();
  return 0;
}
