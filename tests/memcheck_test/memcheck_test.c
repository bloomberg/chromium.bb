/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
  Test integration of Valgrind and NaCl.
*/

#include <stdio.h>
#include <stdlib.h>

/* For NaCl we have to use a special verion of valgrind.h */
#include "native_client/src/third_party/valgrind/nacl_valgrind.h"

#define SHOW_ME fprintf(stderr, "========== %s:%d =========\n",\
                           __FUNCTION__, __LINE__)

/* Use the memory location '*a' in a conditional statement */
void use(int *a) {
  if (*a == 777) { /* Use *a in a conditional statement. */
    fprintf(stderr, "777\n");
  }
}

/* write something to '*a' */
void update(int *a) {
  *a = 1;
}

/* read from heap memory out of bounds */
void oob_read_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  use(foo+10);
  free(foo);
}

/* write to heap memory out of bounds */
void oob_write_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  update(foo+10);
  free(foo);
}

/* read uninitialized value from heap */
void umr_heap_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  use(foo+5);
  free(foo);
}

/* read uninitialized value from stack */
void umr_stack_test() {
  int foo[10];
  SHOW_ME;
  use(foo+5);
}

/* use heap memory after free() */
void use_after_free_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  foo[5] = 666;
  free(foo);
  use(foo+5);
}

/* memory leak */
void leak_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  if (!foo) exit(0);
}

/* run all tests */
int main() {
  if (!RUNNING_ON_VALGRIND) {
    /* Don't run this test w/o valgrind. The run-time library might be smart
     enough to catch some of these bugs and crash. */
    return 0;
  }
  if (1) leak_test();
  if (1) oob_read_test();
  if (1) oob_write_test();
  if (1) umr_heap_test();
  if (1) umr_stack_test();
  if (0) use_after_free_test();
  SHOW_ME;
  return 0;
}
