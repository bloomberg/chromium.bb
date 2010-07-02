/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl test for super simple program not using newlib:
 * This test exerices the 64bit int data types.
 */

#include "barebones.h"

/* Uncommenting this currently leads to undefined symbols on all platforms */
/* error: undefined reference to '__(u)divdi3' */

/* #define USE_DIVIDE */

int main(int argc, char* argv[]) {
  int failures = 0;

  int i;
  long long sa;
  long long sb;
  long long sc;

  /* TODO(robertm): make signedness matter */
  unsigned long long ua;
  unsigned long long ub;
  unsigned long long uc;

  if (THIS_IS_ALWAYS_FALSE_FOR_SMALL_NUMBERS(argc)) {
    /* should never happens - but confuses optimizer */
    myprint("THIS SHOULD NEVER HAPPEN\n");
    sa = argc;
    sb = argc;

    ua = argc;
    ub = argc;
  } else {
    sa = 0x12345678;
    sb = 0x1;
    ua = 0x12345678;
    ub = 0x1;
  }

  for (i = 0; i < 16; ++i) {
    sc = sa * sb;
    if (sc >> i != sa) {
      ++failures;
      myprint("failure in signed mult\n");
    }


#if defined(USE_DIVIDE)
    sc += failures; /* hopefully a nop */
    sc = sc / sb;

    if (sc != sa) {
      ++failures;
      myprint("failure in signed div\n");
    }
#endif

    uc = ua * ub;
    if (uc >> i != ua) {
      ++failures;
      myprint("failure in unsigned mult\n");
    }

#if defined(USE_DIVIDE)
    uc += failures; /* hopefully a nop */
    uc = uc / ub;

    if (uc != ua) {
      ++failures;
      myprint("failure in unsigned div\n");
    }
#endif

    sb <<= 1;
    ub <<= 1;
  }

  NACL_SYSCALL(exit)(failures + 55);

  /* UNREACHABLE */
  return 0;
}
