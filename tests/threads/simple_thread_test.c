/*
 * Copyright 2009, Google Inc.
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

/*
 * simple test for NativeClient threads
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define kNumThreads 2
#define kNumRounds 10
#define kHelloWorldString "hello world\n"


/* tls */
__thread int tls_var_data_int = 666;
__thread char tls_var_data_string[100] = kHelloWorldString;


__thread int tls_var_bss_int;
__thread char tls_var_bss_string[100];

int GlobalCounter = 0;

AtomicWord GlobalError = 0;


void IncError() {
  AtomicIncrement(&GlobalError, 1);
}


#define USE_PTHREAD_LIB
#if defined(USE_PTHREAD_LIB)

pthread_mutex_t mutex;


void Init() {
  pthread_mutex_init(&mutex, NULL);
}


void CriticalSectionEnter() {
  pthread_mutex_lock(&mutex);
}


void CriticalSectionLeave() {
  pthread_mutex_unlock(&mutex);
}

#else

AtomicWord GlobalMutex = 0;


void Init() {
}


void CriticalSectionEnter() {
  int old_value;
  do {
    old_value = AtomicExchange(&GlobalMutex, 1);
  } while (1 == old_value);
}


void CriticalSectionLeave() {
  AtomicExchange(&GlobalMutex, 0);
}

#endif


void CounterWait(int id, int round) {
  for (;;) {
    CriticalSectionEnter();
    if (round * kNumThreads + id == GlobalCounter) {
      CriticalSectionLeave();
      break;
    }
    CriticalSectionLeave();
  }
}


void CounterAdvance(int id, int round) {
  CriticalSectionEnter();
  GlobalCounter = round * kNumThreads + id + 1;
  CriticalSectionLeave();
}


void CheckAndUpdateTlsVars(int id, int round) {
  if (0 == round) {
    if (666 != tls_var_data_int ||
        0 != strcmp(tls_var_data_string, kHelloWorldString)) {
      printf("[%d] ERROR: bad initial tls data\n", id);
      IncError();
    }

    if ( 0 != tls_var_bss_int ||
        0 != strcmp(tls_var_bss_string, "")) {
      printf("[%d] ERROR: bad initial tls bss\n", id);
      IncError();
    }
  } else {
    if (round * kNumThreads + id != tls_var_data_int ||
        0 != strcmp(tls_var_data_string, kHelloWorldString)) {
      printf("[%d] ERROR: bad tls data\n", id);
      IncError();
    }

    if (round * kNumThreads + id != tls_var_bss_int ||
        0 != strcmp(tls_var_bss_string, kHelloWorldString)) {
      printf("[%d] ERROR: bad tls bss\n", id);
      IncError();
    }
  }

  /* prepare tls data for next round */
  tls_var_data_int = (round + 1) * kNumThreads + id;
  tls_var_bss_int = (round + 1) * kNumThreads + id;
  strcpy(tls_var_bss_string, tls_var_data_string);
}


void* MyThreadFunction(void* arg) {
  int id = *(int *)arg;
  int r;

  printf("[%d] entering thread\n", id);
  if (id < 0 || id >= kNumThreads) {
    printf("[%d] ERROR: bad id\n", id);
    return 0;
  }

  for (r = 0; r < kNumRounds; ++r) {
    printf("[%d] before round %d\n", id, r);
    CounterWait(id, r);
    CheckAndUpdateTlsVars(id, r);
    CounterAdvance(id, r);
    printf("[%d] after round %d\n", id, r);
  }

  printf("[%d] exiting thread\n", id);
  return 0;
}


int main(int argc, char *argv[]) {
  pthread_t tid[kNumThreads];
  int ids[kNumThreads];
  int i = 0;

  Init();

  for (i = 0; i < kNumThreads; ++i) {
    int rv;
    ids[i] = i;
    printf("creating thread %d\n", i);
    rv = pthread_create(&tid[i], NULL, MyThreadFunction, &ids[i]);
    if (rv != 0) {
      printf("ERROR: in thread creation\n");
      IncError();
    }
  }

  for (i = 0; i < kNumThreads; ++i) {
    pthread_join(tid[i], NULL);
  }

  return GlobalError;
}
