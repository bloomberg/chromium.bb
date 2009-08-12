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

#include "native_client/src/include/portability.h"

int loop_ffs(int v) {
  int rv = 1;
  int mask = 1;

  while (0 != mask) {
    if (v & mask) {
      return rv;
    }
    mask <<= 1;
    ++rv;
  }
  return 0;
}

/*
 * This exhaustively tests all 32-bit numbers, so will take a while
 * (about a minute on current machines).
 */
int main(int ac,
         char **av) {
  unsigned int errors = 0;
  uint32_t x;

  for (x = 1; 0 != x; ++x) {
    if (loop_ffs(x) != ffs(x)) {
      printf("ERROR: differs at %d (0x%x)\n", x, x);
      errors = 1;  /* if fail everywhere, errors would be UINT_MAX */
    }
  }
  if (loop_ffs(0) != ffs(0)) {
    printf("ERROR: differs at 0\n");
    errors = 1;
  }
  return errors;
}
