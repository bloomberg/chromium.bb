/*
 * Copyright 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/nacl_assert.h"

int main(int argc, char *argv[]) {
  const char *env_var = getenv("FOO");
  if (strcmp(argv[1], "with_p") == 0) {
    ASSERT_NE_MSG(env_var, NULL, "expected FOO=bar");
    ASSERT_EQ_MSG(strcmp(env_var, "bar"), 0, "error: expected FOO=bar");
  } else {
    ASSERT_EQ_MSG(env_var, NULL, "FOO set in environment");
  }
  return 0;
}
