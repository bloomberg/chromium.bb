/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client pthreads implementation layer
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_PTHREAD_TYPES_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_PTHREAD_TYPES_H_ 1

#include "native_client/src/untrusted/pthread/nc_hash.h"
#include "native_client/src/untrusted/pthread/pthread.h"


#ifdef __cplusplus
extern "C" {
#endif

void nc_token_init(volatile int *token);
int  nc_token_acquire(volatile int *token);
void nc_token_release(volatile int *token);

int  nc_pthread_condvar_ctor(pthread_cond_t * cond);

typedef enum {
  THREAD_RUNNING,
  THREAD_TERMINATED
} nc_thread_status_t;

typedef struct entry {
  STAILQ_ENTRY(entry) entries;  /* a pointer - 4 bytes */
  int32_t is_used;              /* 4 bytes */
  int32_t size;                 /* 4 bytes */
  /* The whole structure should be 32 bytes to keep the alignment. */
  int32_t padding[5];              /* 4 * 5 = 20 bytes */
} nc_thread_memory_block_t;

typedef enum {
  THREAD_STACK_MEMORY = 0,
  TLS_AND_TDB_MEMORY,
  MAX_MEMORY_TYPE
} nc_thread_memory_block_type_t;

struct tsd;
struct nc_basic_thread_data;

/* This struct defines the layout of the TDB */
typedef struct {
  void *tls_base;  /* tls accesses are made relative to this base */
  struct tsd *thread_specific_data;  /* used for set/get_specific */
  int joinable;
  int join_waiting;
  nc_thread_memory_block_t *stack_node;
  nc_thread_memory_block_t *tls_node;
  nc_thread_function  start_func;
  void* state;
  int exiting_without_returning;
  struct nc_basic_thread_data *basic_data;
} nc_thread_descriptor_t;

typedef struct nc_basic_thread_data {
  pthread_t thread_id;  /* thread id */
  void *retval;
  nc_hash_entry_t hash_entry;
  nc_thread_status_t status;
  pthread_cond_t join_condvar;
  /* pointer to the tdb, will be null after the thread terminates */
  nc_thread_descriptor_t *tdb;
} nc_basic_thread_data_t;


#define MEMORY_BLOCK_ALLOCATION_SIZE(real_size) \
  (sizeof(nc_thread_memory_block_t) + (real_size))

/* Structures for handling thread-specific data.  */

typedef struct {
  void (*destruction_callback)(void *p);
  unsigned int generation;
} tsd_status_t;

/* A key is free when its generation is even (starting with 0). */
#define KEY_ALLOCATED(Generation) (((Generation) & 1) == 1)

typedef struct tsd {
  unsigned int generation;
  void *ptr;
} tsd_t;

#define INVALID_HANDLE (-1)

#define PTHREAD_DESTRUCTOR_ITERATIONS 4

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_NC_PTHREAD_TYPES_H_ */
