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
 * This tests the sysconf() system call.
 * It verifies that it returns the correct values for the supported query
 * names and sets errno to EINVAL otherwise.
 */

int main(void) {
  int result, pagesize;
  char arg_conf_name[8];
  char *argv[2] = { "sysconf", arg_conf_name };

  /* Prepare the sandbox. */
  MinsfiInitializeSandbox();

  /* Check that PAGESIZE query is enabled. */
  pagesize = sysconf(_SC_PAGESIZE);
  ASSERT_NE(-1, pagesize);

  sprintf(arg_conf_name, "%d", MINSFI_SYSCONF_PAGESIZE);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(pagesize, result);

  /*
   * Check some unsupported values. The sysconf() call should fail and
   * the sandbox should return (-errno). Note that we are checking the value
   * of errno inside the sandboxed environment.
   */
  sprintf(arg_conf_name, "%d", -1);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(-EINVAL, result);

  sprintf(arg_conf_name, "%d", 0);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(-EINVAL, result);

  sprintf(arg_conf_name, "%d", 1);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(-EINVAL, result);

  sprintf(arg_conf_name, "%d", 3);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(-EINVAL, result);

  sprintf(arg_conf_name, "%d", 1024);
  result = MinsfiInvokeSandbox(2, argv);
  ASSERT_EQ(-EINVAL, result);

  /* Clean up. */
  MinsfiDestroySandbox();

  return 0;
}
