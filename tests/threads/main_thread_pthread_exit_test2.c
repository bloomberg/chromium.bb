/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test checks exiting main thread before other threads are finished.
 * Unfortunately, there is no way to join to main thread. So this test
 * sometimes fails to cover this scenario.
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/nacl_syscalls.h>
#include <time.h>

void* ThreadFunction(void* arg) {
  struct timespec time;
  int rv;
  time.tv_sec = 0;
  time.tv_nsec = 500000000;
  /* Sleep half a second taking early wakeups into account. */
  while (-1 == (rv = nanosleep(&time, &time)) && EINTR == errno) ;
  return NULL;
}

int main(void) {
  pthread_t tid;
  int rv;
  rv = pthread_create(&tid, NULL, ThreadFunction, NULL);
  if (rv != 0) {
    fprintf(stderr, "Error: in thread creation %d\n", rv);
    return 1;
  }
  pthread_exit(NULL);
  /* Should never get here. */
  return 2;
}