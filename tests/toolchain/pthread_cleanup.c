/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_variable = 0;

static void cleanup(void *arg) {
  *(int *) arg = 1;
}

/*
 * Test both that pthread_cleanup_push/pop works as it should, and that it
 * doesn't clobber nearby stack, as seen in
 * http://code.google.com/p/nativeclient/issues/detail?id=2490
 */

#define GUARD_MAGIC 0xdeadbeef

static void *thread_func(void *arg) {
  volatile int guard = GUARD_MAGIC;
  pthread_cleanup_push(&cleanup, &test_variable);
  pthread_cleanup_pop(1);
  if (guard != GUARD_MAGIC) {
    fprintf(stderr, "guard clobbered to %#x\n", guard);
    exit(2);
  }
  return NULL;
}

int main(void) {
  pthread_t th;
  int error = pthread_create(&th, NULL, &thread_func, NULL);
  if (error != 0) {
    fprintf(stderr, "pthread_create: %s\n", strerror(error));
    return 1;
  }
  error = pthread_join(th, NULL);
  if (error != 0) {
    fprintf(stderr, "pthread_join: %s\n", strerror(error));
    return 1;
  }
  return test_variable == 1 ? 0 : 2;
}
