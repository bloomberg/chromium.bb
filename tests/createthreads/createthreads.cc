/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// createthreads test
//   create & exit a whole bunch of threads
//   tests for memory, thread and resource recycling

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//const int NC_MAX_THREADS = 512;

const int kJoinLoopCount = 2500;
const int kDelayedJoinInnerLoopCount = (NC_MAX_THREADS - 1);
const int kDelayedJoinLoopCount = 20;
const int kDetachLoopCount = (NC_MAX_THREADS - 1);
int g_create;

void* ThreadEntry(void *userdata) {
  // do nothing and immediately exit
  return userdata;
}


void Fail() {
  printf("Main: Unable to create thread!\n");
  printf("Main: [After %d calls to pthread_create()]\n", g_create);
  printf("TEST FAILED\n");
  exit(-1);
}


/* creates and waits via pthread_join() for thread to exit */
void Test0() {
  pthread_t thread_id;
  void *thread_ret;
  int p = pthread_create(&thread_id, NULL, ThreadEntry, NULL);
  if (0 != p)
    Fail();
  else
    g_create++;
  /* wait for thread to exit */
  pthread_join(thread_id, &thread_ret);
}


/* creates, but does not wait for thread to exit */
void Test1a(int i, pthread_t *thread_id) {
  int p = pthread_create(&thread_id[i], NULL, ThreadEntry, NULL);
  if (0 != p)
    Fail();
  else
    g_create++;
  /* join up after a large block of threads created */
}


void Test1() {
  pthread_t thread_id_array[kDelayedJoinInnerLoopCount];
  void *thread_ret;
  for (int i = 0; i < kDelayedJoinInnerLoopCount; ++i)
    Test1a(i, thread_id_array);
  for (int i = 0; i < kDelayedJoinInnerLoopCount; ++i) {
    pthread_join(thread_id_array[i], &thread_ret);
  }
}


/* creates as detached thread, cannot join */
void Test2() {
  pthread_t thread_id;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int p = pthread_create(&thread_id, NULL, ThreadEntry, NULL);
  if (0 != p)
    Fail();
  else
    g_create++;
  /* cannot join on detached thread */
}


int main(int argc, char **argv) {

#if 0
  // todo: enable for nacl when defined in limits.h
  printf("Max threads PTHREAD_THREADS_MAX: %d\n", PTHREAD_THREADS_MAX);
  printf("Max threads sysconf(_SC_THREAD_THREADS_MAX): %d\n", sysconf(_SC_THREAD_THREADS_MAX));
#else
  printf("Max threads NC_THREADS_MAX: %d\n", NC_MAX_THREADS);
#endif

  printf("Test0() creating & joining threads...\n");
  for (int i = 0; i < kJoinLoopCount; ++i)
    Test0();
  printf("Test0() completed\n");

  printf("Test1() create batch & join batch...\n");
  for (int i = 0; i < kDelayedJoinLoopCount; ++i)
    Test1();
  printf("Test1() completed\n");

  printf("Test2() create detached threads...\n");
  for (int i = 0; i < kDetachLoopCount; ++i)
    Test2();
  printf("Test2() completed\n");
  printf("%d calls to pthread_create() were made\n", g_create);

  printf("TEST PASSED\n");
  return 0;
}
