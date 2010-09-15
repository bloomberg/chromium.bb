/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef _UNTRUSTED_POSIX_OVER_SRPC_POSIX_OVER_SRPC_H_
#define _UNTRUSTED_POSIX_OVER_SRPC_POSIX_OVER_SRPC_H_

#include <limits.h>
#include <pthread.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

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
#define INITIAL_FD_LIST_SIZE 128

/*
 * This enum defines values for posix calls ids. Posix call id is field of
 * __psrpc_request_t structure.
 */
enum posix_call_id {
  PSRPC_ACCEPT,
  PSRPC_BIND,
  PSRPC_CLOSE,
  PSRPC_CLOSEDIR,
  PSRPC_GETCWD,
  PSRPC_GETPAGESIZE,
  PSRPC_LISTEN,
  PSRPC_OPEN,
  PSRPC_OPENDIR,
  PSRPC_PATHCONF,
  PSRPC_PIPE,
  PSRPC_READDIR,
  PSRPC_SETSOCKOPT,
  PSRPC_SOCKET,
  PSRPC_TIMES,

  PSRPC_EXIT_HOOK
};


/*
 * It is necessary to keep some security token (int value) with every opened
 * socket file descriptor. This structure keeps these pairs.
 */
typedef struct {
  int nacl_fd;
  int key;
} __psrpc_fd_info_t;

/* This is table of all __psrpc_fd_info_t structures. */
typedef struct {
  __psrpc_fd_info_t* body;
  int size;
  int top;
  pthread_mutex_t mu;
} __psrpc_fd_list_t;

extern __psrpc_fd_list_t __psrpc_fd_list;

int __psrpc_fd_list_push(__psrpc_fd_list_t* fd_list, int nacl_fd, int key);
int __psrpc_fd_list_get_key(__psrpc_fd_list_t* fd_list, int nacl_fd);
int __psrpc_fd_list_remove(__psrpc_fd_list_t* fd_list, int nacl_fd);

#define fd_list_push(nacl_fd, key)                                             \
  __psrpc_fd_list_push(&__psrpc_fd_list, nacl_fd, key)
#define fd_list_remove(nacl_fd)                                                \
  __psrpc_fd_list_remove(&__psrpc_fd_list, nacl_fd)
#define fd_list_get_key(nacl_fd)                                             \
  __psrpc_fd_list_get_key(&__psrpc_fd_list, nacl_fd)

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
int __psrpc_request_destroy(__psrpc_request_t* request);

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
int __psrpc_queue_push(__psrpc_queue_t* queue, __psrpc_request_t* request);

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
