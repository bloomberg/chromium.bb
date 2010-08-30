/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Here is all logic of posix_over_srpc library in this file.
 * Nacl module linked with this library can be executed only by special
 * launcher.
 *
 * There is constructor in this library:
 * void __psrpc_wait_for_upcall_handler() __attribute__((constructor(101)));
 * This constructor waits on
 * pthread_cond_wait(__psrpc_init_cv, __pthread_init_mu) until
 * __psrpc_upcall_handler makes pthread_cond_signal(__psrpc_init_cv).
 * After signal received normal execution of nacl module continues.
 *
 * After __psrpc_upcall_handler started it is possible to make posix calls
 * via srpc. In a few words handler makes following things in a loop:
 *   1) Waits on pthread_cond_wait(__psrpc_handler_cv, __psrc_handler_mu).
 *   2) When somebody gives him a signal handler awakes, takes the call
 *      from __psrpc_queue, completes it via srpc.
 *   3) Sends signal to corresponding procedure, from which handler was
 *      awakened.
 *   4) Goes to pthread_cond_wait() again.
 *
 * What happens when somebody makes posix call from nacl module.
 *   1) Posix call creates its own exclusive mutex and condition variable
 *      and locks mutex.
 *   2) After that locks __psrpc_handler_mu mutex. It might happen
 *      only when handler is on cond_wait.
 *   3) Puts its request in __psrpc_queue.
 *   4) Sends signal to handler and releases __psrpc_handler_mu mutex.
 *   5) Waits on it own condition variable for signal from handler.
 *   6) After that just cleans everything up and returns result.
 *
 * The most controversial part is "When __psrpc_upcall_handler exits?".
 * Now handler terminates when all threads created in nacl module are done
 * and main() procedure finished and main thread waits
 * in __srpc_wait or __pthread_shutdown for handler completion.
 * If main thread is done the magic number of threads meaning
 * handler have to exit is 4.
 */

#include "posix_over_srpc.h"

#include <malloc.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

NaClSrpcChannel* up_channel;

__psrpc_queue_t __psrpc_queue;

volatile int __psrpc_number_of_threads;
volatile int __psrpc_main_thread_completion_flag = 0;

pthread_mutex_t __psrpc_handler_mu;
pthread_cond_t __psrpc_handler_cv;
pthread_mutex_t __psrpc_init_mu;
pthread_cond_t __psrpc_init_cv;

/*
 * Srpc invocation signatures for posix calls. Their order have to correspond
 * to enum posix_call_id.
 */
char* __psrpc_signatures[] = {
  "closedir:i:ii",
  "getcwd::Cii",
  "getpagesize::i",
  "open:sii:hi",
  "opendir:s:ii",
  "pathconf:si:ii",
  "pipe::iihh",
  "readdir:i:iiisii",
  "times::iiii",
  /* Signature for PSRPC_EXIT_HOOK */
  "::"
};

/*
 * Makes srpc invoke to handle posix call request.
 */
void __psrpc_request_invoke(__psrpc_request_t* request) {
  NaClSrpcError ret_code;
  char* signature = __psrpc_signatures[request->upcall_id];

  ret_code = NaClSrpcInvokeV(up_channel, request->upcall_id + 1,
                             request->args[0], request->args[1]);
  if (ret_code != NACL_SRPC_RESULT_OK) {
    printf("%s srpc invocation code: %s\n", signature,
           NaClSrpcErrorString(ret_code));
  }
}

/*
 * Passes chunk of memory pointed by ptr of given size to NaClSrpcArg.
 * There is no memory copying, so you cannot change or free this memory until
 * NaClSrpcArg is no longer used.
 */
void __psrpc_pass_memory(NaClSrpcArg* arg, void* ptr, size_t size) {
  arg->u.caval.count = size;
  arg->u.caval.carr = (char*)ptr;
}

/*
 * Allocates memory for input and output parameters of posix call.
 * Sets arguments' tags according to call's signature.
 * Initializes associated with request mutex and condition variable.
 * Returns 0 on success, 1 on error.
 */
int __psrpc_request_create(__psrpc_request_t* request, int id) {
  int y;
  const char* arg_types = strchr(__psrpc_signatures[id], ':');
  const char* ret_types = strchr(arg_types + 1, ':');
  const char* null_symbol = strchr(ret_types + 1, '\0');
  request->upcall_id = id;
  request->ninputs = ret_types - arg_types - 1;
  request->noutputs = null_symbol - ret_types - 1;
  pthread_mutex_init(&request->caller_mu, NULL);
  pthread_cond_init(&request->caller_cv, NULL);
  request->args[0] = calloc(request->ninputs + 1, sizeof(*request->args[0]));
  if (NULL == request->args[0]) {
    goto clear0;
  }
  request->args[1] = calloc(request->noutputs + 1, sizeof(*request->args[1]));
  if (NULL == request->args[1]) {
    goto clear1;
  }
  request->args[0][0] = calloc(request->ninputs + 1,
                               sizeof(*request->args[0][0]));
  if (NULL == request->args[0][0]) {
    goto clear2;
  }
  for (y = 1; y < request->ninputs; ++y) {
    request->args[0][y] = request->args[0][0] + y;
  }
  request->args[1][0] = calloc(request->noutputs + 1,
                               sizeof(*request->args[1][0]));
  if (NULL == request->args[1][0]) {
    goto clear3;
  }
  for (y = 1; y < request->noutputs; ++y) {
    request->args[1][y] = request->args[1][0] + y;
  }
  request->args[0][request->ninputs] = NULL;
  request->args[1][request->noutputs] = NULL;
  for (++arg_types, y = 0; arg_types != ret_types; ++arg_types, ++y) {
    request->args[0][y]->tag = (enum NaClSrpcArgType)*arg_types;
  }
  for (++ret_types, y = 0; ret_types != null_symbol; ++ret_types, ++y) {
    request->args[1][y]->tag = (enum NaClSrpcArgType)*ret_types;
  }
  return 0;

 clear3:
  free(request->args[0][0]);
 clear2:
  free(request->args[1]);
 clear1:
  free(request->args[0]);
 clear0:
  return 1;
}

/*
 * Frees memory allocated by __psrpc_request_create.
 * Destroys corresponding mutex and condition variable.
 */
void __psrpc_request_destroy(__psrpc_request_t* request) {
  if (NULL == request) {
    return;
  }
  free(request->args[0][0]);
  free(request->args[1][0]);
  free(request->args[0]);
  free(request->args[1]);
  pthread_mutex_destroy(&request->caller_mu);
  pthread_cond_destroy(&request->caller_cv);
}

/*
 * Following 4 methods implement queue on array.
 */
void __psrpc_queue_init(__psrpc_queue_t* queue) {
  queue->front = queue->back = 0;
  queue->size = INITIAL_QUEUE_SIZE;
  queue->body = (__psrpc_request_t**)calloc(INITIAL_QUEUE_SIZE,
                                            sizeof(*queue->body));
}

int __psrpc_queue_empty(__psrpc_queue_t* queue) {
  return (queue->front == queue->back);
}

__psrpc_request_t* __psrpc_queue_pop(__psrpc_queue_t* queue) {
  __psrpc_request_t* result = queue->body[queue->front];
  ++queue->front;
  if (queue->front == queue->size) {
    queue->front = 0;
  }
  return result;
}

void __psrpc_queue_push(__psrpc_queue_t* queue, __psrpc_request_t* request) {
  queue->body[queue->back] = request;
  ++queue->back;
  if (queue->back == queue->size) {
    queue->back = 0;
  }
  if (queue->back == queue->front) {
    /*
     * Queue overflowed. There is no more space for one more push.
     * Thats why making size of queue 2 times bigger.
     */
    queue->body = (__psrpc_request_t**)realloc(queue->body,
                                               sizeof(*queue->body)
                                               * queue->size * 2);
    memcpy(&queue->body[queue->size], queue->body,
           queue->back * sizeof(__psrpc_request_t*));
    queue->back += queue->size;
    queue->size *= 2;
  }
}

/*
 * This procedure implements such steps of posix call:
 *  1) It puts request into queue.
 *  2) Waits until call is done.
 * It could be called right after request is created and input parameters of
 * posix call are putted into request.args[0].
 */
void __psrpc_make_call(__psrpc_request_t* request) {
  pthread_mutex_lock(&request->caller_mu);
  pthread_mutex_lock(&__psrpc_handler_mu);
  __psrpc_queue_push(&__psrpc_queue, request);
  pthread_cond_signal(&__psrpc_handler_cv);
  pthread_mutex_unlock(&__psrpc_handler_mu);
  pthread_cond_wait(&request->caller_cv, &request->caller_mu);
  pthread_mutex_unlock(&request->caller_mu);
}

/*
 * Makes special request with upcall_id=PSRPC_EXIT_HOOK.
 * Also refreshes information about number of threads and status of main thread.
 * Every time handler receives request with such id, it checks for exit
 * condition one more time.
 */
void __thread_exit_hook(int is_main_thread, int nthreads) {
  if (is_main_thread) {
    __psrpc_main_thread_completion_flag = 1;
  }
  __psrpc_number_of_threads = nthreads;

  __psrpc_request_t request;
  __psrpc_request_create(&request, PSRPC_EXIT_HOOK);
  __psrpc_make_call(&request);
  __psrpc_request_destroy(&request);
}

void __srpc_wait_hook() {
  pthread_exit(NULL);
}

void __srpc_init();

/*
 * Calls __srpc_init to make srpc interaction possible. After that
 * blocks normal execution of nacl module until init_upcall_channel
 * is called via srpc.
 */
void __attribute__((constructor(101))) __psrpc_wait_for_upcall_handler() {
  atexit(__srpc_wait_hook);
  pthread_mutex_init(&__psrpc_init_mu, NULL);
  pthread_cond_init(&__psrpc_init_cv, NULL);

  pthread_mutex_lock(&__psrpc_init_mu);
  __srpc_init();
  /* Wait for init_upcall_channel to be called via srpc. */
  pthread_cond_wait(&__psrpc_init_cv, &__psrpc_init_mu);
  pthread_mutex_unlock(&__psrpc_init_mu);

  pthread_mutex_destroy(&__psrpc_init_mu);
  pthread_cond_destroy(&__psrpc_init_cv);
}

/*
 * This method handles posix calls from all threads of nacl module.
 * When all threads spawned from main() are done it exits.
 * See beginning of this file for more explanations.
 */
void __psrpc_upcall_handler() {
  __psrpc_queue_init(&__psrpc_queue);
  pthread_mutex_init(&__psrpc_handler_mu, NULL);
  pthread_cond_init(&__psrpc_handler_cv, NULL);
  pthread_mutex_lock(&__psrpc_handler_mu);

  pthread_mutex_lock(&__psrpc_init_mu);
  pthread_cond_signal(&__psrpc_init_cv);
  pthread_mutex_unlock(&__psrpc_init_mu);

  int exit_flag = 0;
  __psrpc_request_t* request;
  for (; !exit_flag;) {
    pthread_cond_wait(&__psrpc_handler_cv, &__psrpc_handler_mu);
    for (; !__psrpc_queue_empty(&__psrpc_queue);) {
      request = __psrpc_queue_pop(&__psrpc_queue);
      if (request->upcall_id == PSRPC_EXIT_HOOK) {
        if (__psrpc_main_thread_completion_flag
            && (__psrpc_number_of_threads <= 4)) {
          exit_flag = 1;
        }
      } else {
        __psrpc_request_invoke(request);
      }
      pthread_mutex_lock(&request->caller_mu);
      pthread_cond_signal(&request->caller_cv);
      pthread_mutex_unlock(&request->caller_mu);
    }
  }

  pthread_mutex_unlock(&__psrpc_handler_mu);
  pthread_mutex_destroy(&__psrpc_handler_mu);
  pthread_cond_destroy(&__psrpc_handler_cv);
}

/*
 * Initializes srpc channel for up calls and starts __psrpc_upcall_handler.
 */
NaClSrpcError init_upcall_channel(NaClSrpcChannel* channel,
                                  NaClSrpcArg** in_args,
                                  NaClSrpcArg** out_args) {
  int upcall_imc_handle = in_args[0]->u.ival;
  up_channel = (NaClSrpcChannel*)malloc(sizeof(*up_channel));
  if (!NaClSrpcClientCtor(up_channel, upcall_imc_handle)) {
    printf("init_upcall_channel: unable to create up_channel\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  __psrpc_upcall_handler();
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("init_upcall_channel:i:", init_upcall_channel);
