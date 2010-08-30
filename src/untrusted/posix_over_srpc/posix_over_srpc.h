/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef _UNTRUSTED_POSIX_OVER_SRPC_POSIX_OVER_SRPC_H_
#define _UNTRUSTED_POSIX_OVER_SRPC_POSIX_OVER_SRPC_H_

#include <limits.h>
#include <pthread.h>

#include "nacl/nacl_srpc.h"

#ifndef UNREFERENCED_PARAMETER
  #define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  /* UNREFERENCED_PARAMETER */

/*
 * Srpc channel for up calls. __psrpc_upcall_handler processes all posix calls
 * through it.
 */
extern NaClSrpcChannel* up_channel;

/*
 * Initial size of queue for posix calls requests.
 */
#define INITIAL_QUEUE_SIZE 128

/*
 * This enum defines values for posix calls ids. Posix call id is field of
 * __psrpc_request_t structure.
 */
enum posix_call_id {
  PSRPC_CLOSEDIR,
  PSRPC_GETCWD,
  PSRPC_GETPAGESIZE,
  PSRPC_OPEN,
  PSRPC_OPENDIR,
  PSRPC_PATHCONF,
  PSRPC_PIPE,
  PSRPC_READDIR,
  PSRPC_TIMES,

  PSRPC_EXIT_HOOK
};

/* Posix call request structure. */
typedef struct {
  /* Condition variable and mutex of thread which makes request. */
  pthread_cond_t caller_cv;
  pthread_mutex_t caller_mu;
  /* Input and output arguments of posix call. */
  NaClSrpcArg** args[2];
  /* Id of posix call, look at posix_call_id enum for its values. */
  enum posix_call_id upcall_id;
  /* Amount of input and output parameters of call. */
  int ninputs;
  int noutputs;
} __psrpc_request_t;

int __psrpc_request_create(__psrpc_request_t* request, int id);
void __psrpc_request_destroy(__psrpc_request_t* request);

/* Simple queue on array. */
typedef struct {
  /* Array of queue */
  __psrpc_request_t** body;
  /* Index in array of next element of queue to pop */
  int front;
  /* Index in array where next pushed element should be placed */
  int back;
  /* Size of array */
  int size;
} __psrpc_queue_t;

extern __psrpc_queue_t __psrpc_queue;

int __psrpc_queue_empty(__psrpc_queue_t* queue);
__psrpc_request_t* __psrpc_queue_pop(__psrpc_queue_t* queue);
void __psrpc_queue_push(__psrpc_queue_t* queue, __psrpc_request_t* request);

/*
 * Realizes logic of posix call on side of caller when request structure is
 * initialized and filled.
 */
void __psrpc_make_call(__psrpc_request_t* request);

/*
 * Puts chunk of memory into arg without copying.
 */
void __psrpc_pass_memory(NaClSrpcArg* arg, void* ptr, size_t size);

#endif  /* _UNTRUSTED_POSIX_OVER_SRPC_POSIX_OVER_SRPC_H_ */
