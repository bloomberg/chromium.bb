/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <string.h>
#include <pthread.h>

void foo() {
}

void bar() {
}

void *f1(void *arg) {
  foo();
  bar();
  return NULL;
}

void *f2(void *arg) {
  bar();
  foo();
  return NULL;
}

void test_break_continue_thread() {
  pthread_t t1;
  pthread_t t2;
  int rc;
  rc = pthread_create(&t1, NULL, f1, NULL);
  assert(rc == 0);
  rc = pthread_create(&t2, NULL, f2, NULL);
  assert(rc == 0);
  rc = pthread_join(t1, NULL);
  assert(rc == 0);
  rc = pthread_join(t2, NULL);
  assert(rc == 0);
}

int main(int argc, char **argv) {
  assert(argc >= 2);

  if (strcmp(argv[1], "break_continue_thread") == 0) {
    test_break_continue_thread();
    return 0;
  }
  return 1;
}
