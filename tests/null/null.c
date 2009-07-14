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


#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>


extern void __nacl_null(void);

int main(int  ac,
         char **av) {
  int             opt;
  int             num_rep = 10000000;
  int             i;
  struct timeval  time_start;
  struct timeval  time_end;
  struct timeval  time_elapsed;
  double          time_per_call;

  while (EOF != (opt = getopt(ac, av, "r:"))) switch (opt) {
    case 'r':
      num_rep = strtol(optarg, (char **) NULL, 0);
      break;
    default:
      fprintf(stderr, "Usage: null [-r repetition_count]\n");
      return 1;
  }
  gettimeofday(&time_start, (void *) NULL);
  for (i = num_rep; --i >= 0; ) {
    __nacl_null();
  }
  gettimeofday(&time_end, (void *) NULL);
  time_elapsed.tv_sec = time_end.tv_sec - time_start.tv_sec;
  time_elapsed.tv_usec = time_end.tv_usec - time_start.tv_usec;
  if (time_elapsed.tv_usec < 0) {
    --time_elapsed.tv_sec;
    time_elapsed.tv_usec += 1000000;
  }
  printf("Number of null syscalls: %d\n", num_rep);
  printf("Elapsed time: %d.%06dS\n",
         (int) time_elapsed.tv_sec,
         (int) time_elapsed.tv_usec);
  time_per_call = ((time_elapsed.tv_sec + time_elapsed.tv_usec / 1.0e6)
                   / num_rep);
  printf("Time per call: %gS or %fnS\n",
         time_per_call,
         1.0e9 * time_per_call);
  printf("Calls per sec: %d\n", (int) (1.0 / time_per_call));
  return 0;
}
