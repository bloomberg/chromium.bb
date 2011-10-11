/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/time.h>

__thread int gt_var;

/*
 * Ensure that the compiler won't inline this function and thus get
 * rid of the __tls_get_addr or __tls_read_tp invocations by hoisting
 * it outside of the loop that invokes it.
 */
void bump_tls_var(int amt) __attribute__((noinline));

void bump_tls_var(int amt) {
  /*
   * there should be two calls to __tls_read_tp or __tls_get_addr in
   * the generated code.
   */
  gt_var += amt;
}

void *thread_proc(void *arg) {
  size_t ix;
  size_t upper = (size_t) (uintptr_t) arg;

  /*
   * Compilers will never analyze this and collapse this into one call
   * with the result of the series summation.
   */
  for (ix = 0; ix < upper; ++ix) {
    bump_tls_var(ix);
  }
  return (void *) gt_var;
}

int main(int ac, char **av) {
  size_t num_threads = 1u;
  size_t num_iter = 10000000u;

  int opt;

  struct timeval t_start, t_end, t_elapsed;

  pthread_t *tid;
  size_t thr_ix;

  while (-1 != (opt = getopt(ac, av, "n:t:"))) {
    switch (opt) {
      case 'n':
        num_iter = (size_t) strtoul(optarg, (char **) NULL, 0);
        break;
      case 't':
        num_threads = (size_t) strtoul(optarg, (char **) NULL, 0);
        break;
      default:
        fprintf(stderr, "Usage: tls_perf [-t num_threads] [-n num_iter]\n");
        return 1;
    }
  }

  /*
   * assume no integer overflow.  this is just a test....
   */
  tid = (pthread_t *) malloc(num_threads * sizeof *tid);
  if (NULL == tid) {
    return 2;
  }

  if (0 != gettimeofday(&t_start, (struct timezone *) NULL)) {
    return 3;
  }

  for (thr_ix = 0; thr_ix < num_threads; ++thr_ix) {
    pthread_create(&tid[thr_ix], (const pthread_attr_t *) NULL,
                   thread_proc, (void *) num_iter);
  }

  for (thr_ix = 0; thr_ix < num_threads; ++thr_ix) {
    pthread_join(tid[thr_ix], (void **) NULL);
  }

  if (0 != gettimeofday(&t_end, (struct timezone *) NULL)) {
    return 4;
  }

#if 0
  timersub(&t_end, &t_start, &t_elapsed);
#else
  /* no timersub in newlib */
  t_elapsed.tv_sec = t_end.tv_sec - t_start.tv_sec;
  t_elapsed.tv_usec = t_end.tv_usec - t_start.tv_usec;
  if (t_elapsed.tv_usec < 0) {
    t_elapsed.tv_usec += 1000000;
    --t_elapsed.tv_sec;  /* should not go negative! */
  }
#endif

  printf("\nTest results for TLS performance, %u threads, %u iterations\n\n",
         (unsigned) num_threads, (unsigned) num_iter);
  printf("  start time %12u.%06u\n",
         (unsigned) t_start.tv_sec, (unsigned) t_start.tv_usec);
  printf("    end time %12u.%06u\n",
         (unsigned) t_end.tv_sec, (unsigned) t_end.tv_usec);
  printf("elapsed time %12u.%06u\n",
         (unsigned) t_elapsed.tv_sec, (unsigned) t_elapsed.tv_usec);

  return 0;
}
