/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(HAVE_SDL)
#  include <SDL.h>
#endif

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

int main(int  argc,
         char **argv) {
  char buffer[4096];
  char *bufptr = buffer;
  size_t ix;

  /* type signature of main is constrained by SDL */
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  NACL_ASSERT_IS_POINTER(bufptr);
  NACL_ASSERT_IS_ARRAY(buffer);

  /*
   * NACL_ASSERT_IS_ARRAY(bufptr);
   */

  printf("#buffer = %"PRIuS"\n", NACL_ARRAY_SIZE(buffer));

  /*
   * printf("#bufptr = %lu\n", ARRAY_SIZE(bufptr));
   */

  /*
   * for checking that the store to gNaClArrayCheck is moved out of
   * the loop.
   */
  for (ix = 0; ix < NACL_ARRAY_SIZE(buffer); ++ix) {
    buffer[ix] = ix;
  }
  return (buffer[10] + buffer[4095] == 0);  /* loop was not dead code! */
}
