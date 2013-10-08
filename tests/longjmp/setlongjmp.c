/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <setjmp.h>
#include <stdio.h>

static jmp_buf buf;

int main(void) {
  volatile int result = -1;
  if (!setjmp(buf) ) {
    result = 55;
    printf("setjmp was invoked\n");
    longjmp(buf, 1);
    printf("this print statement is not reached\n");
    return -1;
  } else {
    printf("longjmp was invoked\n");
    return result;
  }
}
