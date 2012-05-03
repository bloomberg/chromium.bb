/* Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "simple.h"

/* provided by main dynamic image */
extern int mywrite(int fd, const void* buf, int n);

char message[] = "IN SHARED LIB\n";

int fortytwo() {
  mywrite(1, message, sizeof message);
  return 42;
}
