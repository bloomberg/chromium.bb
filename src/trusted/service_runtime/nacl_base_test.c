/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
