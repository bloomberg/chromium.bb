/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This exercises the public interface of MinSFI.
 *
 * Internally, we verify that running the Initialize function changes the
 * active sandbox and sets the memory base for the sandboxed code. Destroying
 * the sandbox should reset both to NULL.
 *
 * For the public interface, we check that the Initialize, Invoke and Destroy
 * functions behave correctly when called in the right context, and that they
 * do not crash the program otherwise.
 */

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/nacl_assert.h"

extern uint64_t __sfi_memory_base;

static int argc = 1;
static char *argv[] = { "foo" };

void helper_initialize(void) {
  ASSERT_EQ(true, MinsfiInitializeSandbox());
  ASSERT_NE(NULL, MinsfiGetActiveSandbox());
  ASSERT_NE(0, __sfi_memory_base);
  ASSERT_EQ(__sfi_memory_base, (uintptr_t) MinsfiGetActiveSandbox()->mem_base);
}

void helper_invoke_success(void) {
  ASSERT_EQ((int) 0xCAFEBABE, MinsfiInvokeSandbox(argc, argv));
}

void helper_invoke_error(void) {
  ASSERT_EQ(EXIT_FAILURE, MinsfiInvokeSandbox(argc, argv));
}

void helper_destroy(void) {
  ASSERT_EQ(true, MinsfiDestroySandbox());
  ASSERT_EQ(NULL, MinsfiGetActiveSandbox());
  ASSERT_EQ(0, __sfi_memory_base);
}

int main(void) {
  int i;

  /* Test preconditions. There should be no active sandbox. */
  ASSERT_EQ(NULL, MinsfiGetActiveSandbox());
  ASSERT_EQ(0, __sfi_memory_base);

  /*
   * First, try invoking the sandbox without having initialized it.
   * It should fail but not crash.
   */
  helper_invoke_error();

  /* Initialize, invoke, destroy a couple of times. This should succeed. */
  for (i = 0; i < 3; i++) {
    helper_initialize();
    helper_invoke_success();
    helper_destroy();
  }

  /*
   * Multiple initializations and invokes without destroying. This will leave
   * the sandbox initialized.
   */
  for (i = 0; i < 3; i++) {
    helper_initialize();
    helper_invoke_success();
  }

  /* Now try destroying it multiple times. */
  for (i = 0; i < 3; i++)
    helper_destroy();

  /* Finally, invoking the sandbox after it's been destroyed should fail. */
  helper_invoke_error();
}
