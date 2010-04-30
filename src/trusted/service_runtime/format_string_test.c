/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>

#if defined(HAVE_SDL)
# include <SDL.h>
#endif

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"


int main(int  ac,
         char **av) {
  char buf[100];

  UNREFERENCED_PARAMETER(ac);
  UNREFERENCED_PARAMETER(av);

  sprintf(buf, "%"NACL_PRIxS, (size_t) -1);
  printf("got:%s\n", buf);
  if (sizeof(size_t) == 4) {
    ASSERT_EQ(strcmp(buf, "ffffffff"), 0);
  } else if (sizeof(size_t) == 8) {
    ASSERT_EQ(strcmp(buf, "ffffffffffffffff"), 0);
  } else {
    ASSERT_MSG(0, "Unknown size_t size");
  }
  return 0;
}
