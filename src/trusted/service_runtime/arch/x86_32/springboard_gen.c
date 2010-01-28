/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/springboard.h"


int main() {
  unsigned char * idx = (unsigned char *) &NaCl_springboard;

  printf("unsigned char kSpringboardCode[] = {\n");
  while (idx <= (unsigned char *) &NaCl_springboard_end) {
    printf(" 0x%02x,", *idx);
    idx++;
  }
  printf("\n};\n");

  return 0;
}
