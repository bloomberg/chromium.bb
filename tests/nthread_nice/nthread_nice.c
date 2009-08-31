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

/* NaCl nthread_nice threading test     */
/* Illustrates use of nthread_nice. Build then run without arguments.
 * This test will attempt to create a bunch of threads and then adjust
 * their priorities using the nacl_thread_nice interface. Then they all
 * run in a busy-wait loop trying to increment a counter. The counter
 * value can be used to determine if the priority adjustments had the
 * desired effect on the scheduler.
 *
 * Sadly this test is not extremely conducive to automation, but it
 * does support a couple useful testing scenarios:
 * - Run without arguments, then check the output to confirm that the
 *   higher priority threads are getting more time than the lower
 *   priority threads.
 * - It is a problem if real-time threads can be used to create a denial
 *   of service attack, locking up the machine by saturating the CPU.
 *   You can evaluate this by running on the command line (for example
 *   using make nacl release run) and then trying to kill it with a ^C.
 *   If the program terminates before one of the threads reaches
 *   kMaxIterations, that confirms that the system was able to stop
 *   the real-time threads without them stopping itself. If however
 *   the output shows that one of the threads reached kMaxIterations
 *   then lock-out is probably possible. DO MAKE SURE that the number
 *   of real-time threads is greater than the number of CPU cores.
 *   See the initialization of kNumThreads below.
 *
 */

#define HAVE_THREADS
#include <pthread.h>
#include <stdio.h>

/* TODO(bradchen) rename pthread_nice to nthread_nice once the   */
/* SDK build is settled.                                         */
#define nacl_thread_nice pthread_nice

static int gDone = 0;
static int kMaxIterations = 10;
struct thread_closure {
  int tid;
  int nice;
  int iters;
};

static pthread_mutex_t gTheBigLock;
static pthread_cond_t gDoneCond;
static void Report(struct thread_closure *tc) {
  pthread_mutex_lock(&gTheBigLock);
  printf("thread %d: nice %d; %d iterations\n",
         tc->tid, tc->nice, tc->iters);
  pthread_mutex_unlock(&gTheBigLock);
}

volatile int gDoNotOptimize = 0;
static void DoNothing() {
  gDoNotOptimize += 1;
}

/* Entry point for worker thread. */
void* wWorkerThreadEntry(void *args) {
  struct thread_closure *tc = (struct thread_closure *)args;
  int i;

  nacl_thread_nice(tc->nice);
  pthread_mutex_lock(&gTheBigLock);
  printf("thread %d starting.\n", tc->tid);
  pthread_mutex_unlock(&gTheBigLock);

  do {
    for (i = 0; i < 0x10000000; i++) DoNothing();
    tc->iters += 1;
    if (gDone) break;
    Report(tc);
  } while (tc->iters < kMaxIterations);
  gDone = 1;
  Report(tc);
  pthread_cond_broadcast(&gDoneCond);

  return NULL;
}

#define kNumThreads 12
static pthread_t gThreads[kNumThreads];
static struct thread_closure gTClosure[kNumThreads] =
{ { 0, NICE_BACKGROUND, 0 },
  { 1, NICE_NORMAL, 0 },
  { 2, NICE_REALTIME, 0 },
  { 3, NICE_REALTIME, 0 },
  { 4, NICE_REALTIME, 0 },
  { 5, NICE_REALTIME, 0 },
  { 6, NICE_REALTIME, 0 },
  { 7, NICE_REALTIME, 0 },
  { 8, NICE_REALTIME, 0 },
  { 9, NICE_REALTIME, 0 },
  { 10, NICE_REALTIME, 0 },
  { 11, NICE_REALTIME, 0 },
};
int CreateWorkerThreads() {
  int i;

  for (i = 0; i < kNumThreads; i++) {
    if (0 != pthread_create(&(gThreads[i]), NULL,
                            wWorkerThreadEntry, &(gTClosure[i]))) {
      fprintf(stderr, "pthread_create() failed\n");
      return 0;
    }
  }
  return 1;
}

void RunDemo() {
  pthread_mutex_init(&gTheBigLock, NULL);
  pthread_cond_init(&gDoneCond, NULL);

  pthread_mutex_lock(&gTheBigLock);
  if (CreateWorkerThreads()) {
    nacl_thread_nice(NICE_BACKGROUND);
    while (gDone == 0) pthread_cond_wait(&gDoneCond, &gTheBigLock);
  }
  pthread_mutex_unlock(&gTheBigLock);
}

int main(int argc, char **argv) {
  RunDemo();

  return 0;
}
