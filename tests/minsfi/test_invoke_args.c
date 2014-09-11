/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <string.h>

#include "native_client/src/include/minsfi.h"
#include "native_client/src/include/minsfi_priv.h"
#include "native_client/src/include/nacl_assert.h"

/*
 * Helper function which creates a NULL-terminated string that occupies
 * the given number of bytes.
 */
static inline char *get_str(int bytes) {
  char *str = (char*) malloc(bytes);
  memset(str, 'A', bytes - 1);
  str[bytes - 1] = 0;
  return str;
}

/*
 * Test the CopyArguments function. We verify that it will not attempt to write
 * beyond the bounds of the untrusted stack.
 */
void test_copy_limits(void) {
  const MinsfiSandbox *sb;
  int stack_fit;
  char *argv_fit[1];
  char *argv_info_overflow[1];
  char *argv_arg_overflow[2];

  /* Initialize the sandbox. */
  MinsfiInitializeSandbox();
  sb = MinsfiGetActiveSandbox();

  /* argc < 0 doesn't make sense */
  ASSERT_EQ(0, MinsfiCopyArguments(-5, NULL, sb));

  /*
   * Test that CopyArguments allows to fill the whole stack.
   * The info structure will contain two integers. We cannot invoke the sandbox
   * because it would immediately overflow the stack.
   */
  stack_fit = sb->mem_layout.stack.length - 2 * sizeof(sfiptr_t);
  argv_fit[0] = get_str(stack_fit);
  ASSERT_EQ(sb->mem_layout.stack.offset, MinsfiCopyArguments(1, argv_fit, sb));

  /*
   * Test that CopyArguments fails if the arguments don't leave enough space
   * for the info structure.
   */
  argv_info_overflow[0] = get_str(stack_fit + 1);
  ASSERT_EQ(0, MinsfiCopyArguments(1, argv_info_overflow, sb));
  ASSERT_EQ(EXIT_FAILURE, MinsfiInvokeSandbox(1, argv_info_overflow));

  /*
   * Test that CopyArguments fails if the arguments do not fit onto the stack.
   * The info structure will contain three integers.
   */
  stack_fit = sb->mem_layout.stack.length - 3 * sizeof(sfiptr_t);
  argv_arg_overflow[0] = get_str(stack_fit - 15);
  argv_arg_overflow[1] = get_str(16);
  ASSERT_EQ(0, MinsfiCopyArguments(2, argv_arg_overflow, sb));
  ASSERT_EQ(EXIT_FAILURE, MinsfiInvokeSandbox(2, argv_arg_overflow));

  /* Clean up. */
  MinsfiDestroySandbox();
  free(argv_fit[0]);
  free(argv_info_overflow[0]);
  free(argv_arg_overflow[0]);
  free(argv_arg_overflow[1]);
}

/*
 * This tests whether arguments are correctly passed to the sandbox. We do
 * this by passing it a series of strings containing integer numbers. The
 * sandbox is expected to parse the arguments and return their sum.
 */
void test_arguments_valid(void) {
  char *argv_99[] = { "99" };
  char *argv_1_22_333[] = { "1", "22", "333" };

  /* Prepare the sandbox. */
  MinsfiInitializeSandbox();

  /* Empty arguments. The sandbox should always receive at least one argument
   * (the name of the binary) but we test this anyway. */
  ASSERT_EQ(0, MinsfiInvokeSandbox(0, NULL));

  /* Single argument. */
  ASSERT_EQ(99, MinsfiInvokeSandbox(1, argv_99));

  /* Multiple arguments. */
  ASSERT_EQ(356, MinsfiInvokeSandbox(3, argv_1_22_333));

  /* Clean up. */
  MinsfiDestroySandbox();
}

int main(void) {
  test_copy_limits();
  test_arguments_valid();
  return 0;
}
