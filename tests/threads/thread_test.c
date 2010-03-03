/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Testing suite for NativeClient threads
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof (x) / sizeof (x)[0])

/*
 * On x86, we cannot have more than just shy of 8192 threads running
 * simultaneously.  This is a NaCl architectural limitation.
 * On ARM the limit is 4k.
 * Note that some thread related memory is never freed.
 * On ARM this is quite substantial (about 4kB) and since this test
 * creates quite a few threads, g_num_test_loops cannot be much bigger than 1k.
 * The number of rounds can be changed on the commandline.
 */

int g_num_test_loops  = 10000;

/* Macros so we can use it for array dimensions in ISO C90 */
#define NUM_THREADS 5

__thread int tls_var = 5;

int g_ready = 0;

int g_errors = 0;

int g_verbose = 0;

#define PRINT(cond, mesg) do { if (cond) { \
                                 printf("%s:%d:%d: ", \
                                        __FUNCTION__, \
                                        __LINE__, \
                                        (int)pthread_self()); \
                                 printf mesg; \
                                 fflush(stdout); \
                               }\
                          } while (0)

#define PRINT_ERROR do { PRINT(1, ("Error")); g_errors++; } while (0)

/* TODO(adonovan): display informative errors. */
#define EXPECT_EQ(A, B) do { if ((A)!=(B)) PRINT_ERROR; } while (0)
#define EXPECT_NE(A, B) do { if ((A)==(B)) PRINT_ERROR; } while (0)
#define EXPECT_GE(A, B) do { if ((A)<(B)) PRINT_ERROR; } while (0)
#define EXPECT_LE(A, B) do { if ((A)>(B)) PRINT_ERROR; } while (0)


#define TEST_FUNCTION_START int local_error = g_errors; PRINT(1, ("Start\n"))

#define TEST_FUNCTION_END if (local_error == g_errors) { \
                            PRINT(1, ("OK\n")); \
                          } else { \
                            PRINT(1, ("FAILED\n")); \
                          }


struct SYNC_DATA {
  pthread_mutex_t mutex;
  pthread_cond_t cv;
};


typedef void* (*ThreadFunction)(void *state);


void* FastThread(void *userdata) {
  /* do nothing and immediately exit */
  return 0;
}


/* creates and waits via pthread_join() for thread to exit */
void CreateWithJoin(ThreadFunction func, void *state) {
  pthread_t thread_id;
  void* thread_ret;
  int p = pthread_create(&thread_id, NULL, func, state);
  EXPECT_EQ(0, p);

  /* wait for thread to exit */
  p = pthread_join(thread_id, &thread_ret);
  EXPECT_EQ(0, p);
}


/* creates, but does not wait for thread to exit */
void CreateWithoutJoin() {
  pthread_t thread_id;
  int p = pthread_create(&thread_id, NULL, FastThread, NULL);
  EXPECT_EQ(0, p);
  /* intentionally no pthread_join() */
}


/* creates as detached thread, cannot join */
void CreateDetached() {
  int p;
  pthread_t thread_id;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  p = pthread_create(&thread_id, &attr, FastThread, NULL);
  EXPECT_EQ(0, p);
  /* cannot join on detached thread */
}


void* TlsThread(void* state) {
  struct SYNC_DATA* sync_data = (struct SYNC_DATA*)state;
  PRINT(g_verbose, ("start signal thread: %d\n", tls_var));
  pthread_mutex_lock(&sync_data->mutex);
  tls_var = 8;
  g_ready = 1;
  pthread_cond_signal(&sync_data->cv);
  pthread_mutex_unlock(&sync_data->mutex);
  PRINT(g_verbose, ("terminate signal thread\n"));
  return (void*)33;
}


void TestTlsAndSync() {
  int i = 5;
  void* thread_rv;
  pthread_t thread_id;
  pthread_attr_t attr;
  struct SYNC_DATA sync_data;

  TEST_FUNCTION_START;

  pthread_mutex_init(&sync_data.mutex, NULL);
  pthread_cond_init(&sync_data.cv, NULL);
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  i = pthread_create(&thread_id, &attr, TlsThread, &sync_data);
  EXPECT_EQ(0, i);

  EXPECT_EQ(5, tls_var);
  pthread_mutex_lock(&sync_data.mutex);

  while (!g_ready) {
    pthread_cond_wait(&sync_data.cv, &sync_data.mutex);
  }
  i = pthread_join(thread_id, &thread_rv);
  EXPECT_NE(0, i);  /* join should fail since the thread was created detached */
  EXPECT_EQ(5, tls_var);

  pthread_mutex_unlock(&sync_data.mutex);
  EXPECT_EQ(5, tls_var);
  TEST_FUNCTION_END;
}


void TestManyThreadsJoinable() {
  int i;
  TEST_FUNCTION_START;
  for (i = 0; i < g_num_test_loops; i++) {
    if (i % (g_num_test_loops / 10) == 0) {
      PRINT(g_verbose, ("round %d\n", i));
    }
    CreateWithJoin(FastThread, NULL);
  }
  TEST_FUNCTION_END;
}


void TestManyThreadsDetached() {
  int i;
  TEST_FUNCTION_START;
  for (i = 0; i < g_num_test_loops; i++) {
    if (i % (g_num_test_loops / 10) == 0) {
      PRINT(g_verbose, ("round %d\n", i));
    }
    CreateDetached();
  }
  TEST_FUNCTION_END;
}


void* SemaphoresThread(void *state) {
  sem_t* sem = (sem_t*) state;
  int i = 0, rv;
  for (i = 0; i < g_num_test_loops; i++) {
    rv = sem_wait(&sem[0]);
    EXPECT_EQ(0, rv);
    rv = sem_post(&sem[1]);
    EXPECT_EQ(0, rv);
  }
  EXPECT_EQ(g_num_test_loops, i);
  return 0;
}

void TestSemaphores() {
  int i = 5;
  int rv;
  pthread_t thread_id;
  pthread_attr_t attr;
  sem_t sem[2];
  TEST_FUNCTION_START;
  sem_init(&sem[0], 0, 0);
  sem_init(&sem[1], 0, 0);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  i = pthread_create(&thread_id, &attr, SemaphoresThread, sem);
  EXPECT_EQ(0, i);

  for (i = 0; i < g_num_test_loops; i++) {
    if (i % (g_num_test_loops / 10) == 0) {
      PRINT(g_verbose, ("round %d\n", i));
    }
    rv = sem_post(&sem[0]);
    EXPECT_EQ(0, rv);
    rv = sem_wait(&sem[1]);
    EXPECT_EQ(0, rv);
  }
  sem_destroy(&sem[0]);
  sem_destroy(&sem[1]);
  TEST_FUNCTION_END;
}

void TestSemaphoreInitDestroy() {
  sem_t sem;
  int rv;
  TEST_FUNCTION_START;
  rv = sem_init(&sem, 0, SEM_VALUE_MAX + 1);
  EXPECT_EQ(-1, rv);  /* failure */

  rv = sem_init(&sem, 0, SEM_VALUE_MAX);
  EXPECT_EQ(0, rv);  /* success */
  rv  = sem_destroy(&sem);
  EXPECT_EQ(0, rv);

  rv = sem_init(&sem, 0, 0);
  EXPECT_EQ(0, rv);  /* success */
  rv  = sem_destroy(&sem);
  EXPECT_EQ(0, rv);

  TEST_FUNCTION_END;
}

pthread_once_t once_control = PTHREAD_ONCE_INIT;

typedef int AtomicInt32;  /* Why is this needed? We included pthread already. */

void pthread_once_routine() {
  static AtomicInt32 count = 0;
  AtomicInt32 res =  __sync_fetch_and_add(&count, 1);
  EXPECT_LE(res, 1);
}

void* OnceThread(void *userdata) {
  pthread_once(&once_control, pthread_once_routine);
  return 0;
}


void TestPthreadOnce() {
  int i;
  TEST_FUNCTION_START;
  PRINT(g_verbose, ("creating %d threads\n", g_num_test_loops));
  for (i = 0; i < g_num_test_loops; i++) {
    pthread_t thread_id;
    pthread_attr_t attr;
    int p;
    if (i % (g_num_test_loops / 10) == 0) {
      PRINT(g_verbose, ("round %d\n", i));
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    p = pthread_create(&thread_id, &attr, OnceThread, NULL);
    EXPECT_EQ(0, p);
  }
  TEST_FUNCTION_END;
}

void* RecursiveLockThread(void *state) {
  int rv;
  int i;
  pthread_mutex_t *lock = state;

  for (i = 0; i < g_num_test_loops; ++i) {
    rv = pthread_mutex_lock(lock);
    EXPECT_EQ(0, rv);
  }

  for (i = 0; i < g_num_test_loops; ++i) {
    rv = pthread_mutex_unlock(lock);
    EXPECT_EQ(0, rv);
  }

  return 0;
}

void TestRecursiveMutex() {
  pthread_mutexattr_t attr;
  pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  int rv;
  pthread_t tid[NUM_THREADS];
  int i = 0;
  TEST_FUNCTION_START;

  PRINT(g_verbose, ("starting threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    int rv = pthread_create(&tid[i], NULL, RecursiveLockThread, &mutex);
    EXPECT_EQ(0, rv);
  }

  PRINT(g_verbose, ("joining threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    pthread_join(tid[i], NULL);
  }

  PRINT(g_verbose, ("checking\n"));
  rv = pthread_mutex_lock(&mutex);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_trylock(&mutex);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_unlock(&mutex);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_unlock(&mutex);
  EXPECT_EQ(0, rv);

  rv = pthread_mutex_destroy(&mutex);
  EXPECT_EQ(0, rv);
  memset(&mutex, 0, sizeof(mutex));

  rv = pthread_mutexattr_init(&attr);
  EXPECT_EQ(0, rv);
  rv = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_init(&mutex, &attr);
  EXPECT_EQ(0, rv);

  PRINT(g_verbose, ("starting threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    int rv = pthread_create(&tid[i], NULL, RecursiveLockThread, &mutex);
    EXPECT_EQ(0, rv);
  }

  PRINT(g_verbose, ("joining threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    pthread_join(tid[i], NULL);
  }

  TEST_FUNCTION_END;
}


void TestErrorCheckingMutex() {
  pthread_mutexattr_t attr;
  pthread_mutex_t mutex;
  int rv;

  TEST_FUNCTION_START;
  rv = pthread_mutexattr_init(&attr);
  EXPECT_EQ(0, rv);
  rv = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_init(&mutex, &attr);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_unlock(&mutex);
  EXPECT_NE(0, rv);
  rv = pthread_mutex_lock(&mutex);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_trylock(&mutex);
  EXPECT_NE(0, rv);
  rv = pthread_mutex_unlock(&mutex);
  EXPECT_EQ(0, rv);
  rv = pthread_mutex_unlock(&mutex);
  EXPECT_NE(0, rv);
  TEST_FUNCTION_END;
}


void tsd_destructor(void *arg) {
  *(int*)arg += 1;
}

pthread_key_t tsd_key;


void* TsdThread(void *state) {
  int rv;
  rv = pthread_setspecific(tsd_key, state);
  EXPECT_EQ(0, rv);
  pthread_exit((void*) 5);
  return 0;
}


void TestTSD() {
  int rv;
  void* ptr;
  int destructor_count = 0;
  TEST_FUNCTION_START;
  rv = pthread_key_create(&tsd_key, tsd_destructor);
  EXPECT_EQ(0, rv);

  rv = pthread_setspecific(tsd_key, &rv);
  EXPECT_EQ(0, rv);

  ptr = pthread_getspecific(tsd_key);
  EXPECT_EQ(ptr, &rv);

  CreateWithJoin(TsdThread, &destructor_count);
  EXPECT_EQ(1, destructor_count);

  rv = pthread_key_delete(tsd_key);
  EXPECT_EQ(0, rv);

  TEST_FUNCTION_END;
}

void* MallocThread(void *userdata) {
  void* ptr = 0;
  int i;
  for (i = 0; i < g_num_test_loops; ++i) {
    ptr = (void*) malloc(16);
    EXPECT_NE(NULL, ptr);
  }
  return ptr;
}


void TestMalloc() {
  int i = 0;
  pthread_t tid[NUM_THREADS];
  TEST_FUNCTION_START;

  PRINT(g_verbose, ("starting threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    int rv = pthread_create(&tid[i], NULL, MallocThread, NULL);
    EXPECT_EQ(0, rv);
  }

  PRINT(g_verbose, ("joining threads\n"));
  for (i = 0; i < NUM_THREADS; ++i) {
    void* mem;
    pthread_join(tid[i], &mem);
    free(mem);
  }

  TEST_FUNCTION_END;
}


void* ReallocThread(void *userdata) {
  void* ptr;
  int i;
  ptr = (void*) malloc(16);
  for (i = 0; i < g_num_test_loops; ++i) {
    ptr = (void*)realloc(ptr, 32);
    EXPECT_NE(NULL, ptr);
    ptr = (void*)realloc(ptr, 64000);
    EXPECT_NE(NULL, ptr);
    ptr = (void*)realloc(ptr, 64);
    EXPECT_NE(NULL, ptr);
    ptr = (void*)realloc(ptr, 32000);
    EXPECT_NE(NULL, ptr);
    ptr = (void*)realloc(ptr, 256);
    EXPECT_NE(NULL, ptr);
  }

  return ptr;
}


void TestRealloc() {
  pthread_t tid[NUM_THREADS];
  int i = 0;
  TEST_FUNCTION_START;
  for (i = 0; i < NUM_THREADS; ++i) {
    int rv = pthread_create(&tid[i], NULL, ReallocThread, NULL);
    EXPECT_EQ(0, rv);
  }

  for (i = 0; i < NUM_THREADS; ++i) {
    pthread_join(tid[i], NULL);
  }

  TEST_FUNCTION_END;
}


/* Worker threads should spin-wait for this condition before starting work. */
static volatile int workers_begin;

/* Which intrinsic are we testing? */
static enum { COMPARE_AND_SWAP, FETCH_AND_ADD } intrinsic;

/* Perform 1 million atomic increments of the counter pointed to by
 * |data|, and checks the final result.  Uses the increment strategy
 * specified by the |intrinsic| global. */
static void* WorkerThread(void *data) {
  volatile AtomicInt32* counter = (volatile AtomicInt32*) data;
  int ii;

  /* NB, gets stuck on ARM QEMU. */
  while (!workers_begin)
    ;

  for (ii = 0; ii < 1000000; ++ii) {
    switch (intrinsic) {
      case COMPARE_AND_SWAP:
        /* NB, not atomic on ARM QEMU. */
        for (;;) {
          AtomicInt32 prev = *counter;
          if (__sync_val_compare_and_swap(counter, prev, prev + 1) == prev)
            break;
        }
        break;
      case FETCH_AND_ADD:
        __sync_fetch_and_add(counter, 1);
        break;
      default:
        abort();
    }
  }
  return NULL;
}

/* Runs 10 copies of WorkerThread in parallel.  The address of a
 * shared volatile AtomicInt32 counter is passed to each thread.
 */
static void CheckAtomicityUnderConcurrency() {
  volatile AtomicInt32 counter = 0;
  pthread_t threads[10];
  int ii;

  workers_begin = 0; /* Hold on... */
  for (ii = 0; ii < ARRAY_SIZE(threads); ++ii)
    EXPECT_EQ(0, pthread_create(&threads[ii], NULL, &WorkerThread,
                                (void*) &counter));
  workers_begin = 1; /* Thunderbirds are go! */
  for (ii = 0; ii < ARRAY_SIZE(threads); ++ii)
    EXPECT_EQ(0, pthread_join(threads[ii], NULL));
  EXPECT_EQ(10000000, counter);
}

/* Test hand-written intrinsics for ARM. */
static void TestIntrinsics() {
  TEST_FUNCTION_START;

  /* Test uncontended behaviour: */
  {
    /* COMPARE_AND_SWAP */
    volatile AtomicInt32 x = 123;
    EXPECT_EQ(123, __sync_val_compare_and_swap(&x, 123, 42));  /* matches */
    EXPECT_EQ(42, x);  /* => swapped */
    EXPECT_EQ(42, __sync_val_compare_and_swap(&x, 43, 9876));  /* no match */
    EXPECT_EQ(42, x);  /* => unchanged */

    /* FETCH_AND_ADD */
    x = 123;
    EXPECT_EQ(123, __sync_fetch_and_add(&x, 42));
    EXPECT_EQ(165, x);
    EXPECT_EQ(165, __sync_fetch_and_add(&x, 1));
    EXPECT_EQ(166, x);
  }

  /* Test behaviour with concurrency: */
  intrinsic = COMPARE_AND_SWAP;
  CheckAtomicityUnderConcurrency();

  intrinsic = FETCH_AND_ADD;
  CheckAtomicityUnderConcurrency();

  TEST_FUNCTION_END;
}

int main(int argc, char *argv[]) {
  if (argc == 2) {
    g_num_test_loops = atoi(argv[1]);
  }
  TestTlsAndSync();
  TestManyThreadsJoinable();
  TestManyThreadsDetached();
  TestSemaphores();
  TestSemaphoreInitDestroy();
  TestPthreadOnce();
  TestRecursiveMutex();
  TestErrorCheckingMutex();
  TestTSD();
  TestMalloc();
  TestRealloc();
  TestIntrinsics();

  return g_errors;
}
