/*
 * Copyright 2008, Google Inc.
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

/*
 * Misc functions missing from windows.
 */
#include <stdio.h>
#include <string.h>

#include "native_client/src/include/portability.h"

#if ( DO_NOT_USE_FAST_ASSEMBLER_VERSION_FOR_FFS || defined(_WIN64) )
int ffs(int x) {
  int r = 1;

  if (!x) {
    return 0;
  }
  if (!(x & 0xffff)) {
    /* case 1 */
    x >>= 16;
    r += 16;
  }
  if (!(x & 0xff)) {
    /* case 2 */
    x >>= 8;
    r += 8;
  }
  if (!(x & 0xf)) {
    /* case 3 */
    x >>= 4;
    r += 4;
  }
  if (!(x & 3)) {
    /* case 4 */
    x >>= 2;
    r += 2;
  }
  if (!(x & 1)) {
    /* case 5 */
    x >>= 1;
    r += 1;
  }

  return r;
}
#else
int  ffs(int v) {
  uint32_t rv;

  if (!v) return 0;
  __asm {
    bsf eax, v;
    mov rv, eax;
  }
  return rv + 1;
}
#endif


/*
 * Based on code by Hans Dietrich
 * see http://www.codeproject.com/KB/cpp/xgetopt.aspx
 */

char *optarg;  /* global argument pointer */
int optind = 0;  /* global argv index */

int getopt(int argc, char *argv[], char *optstring) {
  char c, *cp;
  static char *next = NULL;
  if (optind == 0)
    next = NULL;

  optarg = NULL;

  if (next == NULL || *next == ('\0')) {
    if (optind == 0)
      optind++;

    if (optind >= argc ||
        argv[optind][0] != ('-') ||
        argv[optind][1] == ('\0')) {
      optarg = NULL;
      if (optind < argc)
        optarg = argv[optind];
      return EOF;
    }

    if (strcmp(argv[optind], ("--")) == 0) {
      optind++;
      optarg = NULL;
      if (optind < argc)
        optarg = argv[optind];
      return EOF;
    }

    next = argv[optind];
    next++;   /* skip past - */
    optind++;
  }

  c = *next++;
  cp = strchr(optstring, c);

  if (cp == NULL || c == (':'))
    return ('?');

  cp++;
  if (*cp == (':')) {
    if (*next != ('\0')) {
      optarg = next;
      next = NULL;
    } else if (optind < argc) {
      optarg = argv[optind];
      optind++;
    } else {
      return ('?');
    }
  }

  return c;
}
