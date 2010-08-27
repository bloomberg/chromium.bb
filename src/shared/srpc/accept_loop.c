/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"
#include <sys/nacl_syscalls.h>

#define BOUND_SOCKET  3

/* TODO: This file seems to have relative includes and should probably
   not live in shared/   */

struct worker_state {
  int d;
  /*
   * TODO(mseaborn): Remove is_privileged.  Now that the plugin
   * creates only one connection, this concept is unused.
   */
  int is_privileged;
};

static pthread_mutex_t shutdown_wait_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shutdown_wait_cv = PTHREAD_COND_INITIALIZER;
static int shutdown_done = 0;

/*
 * This is weak symbol and can be implemented in other places.
 * posix_over_srpc library implements it since some internal actions
 * of libposix_over_srpc should be performed when execution reaches __srpc_wait.
 */
extern void __srpc_wait_hook() __attribute__((weak));

void __srpc_wait() {
  int       is_embedded;

  if (__srpc_wait_hook) {
    __srpc_wait_hook();
  }
  is_embedded = (srpc_get_fd() != -1);
  if (is_embedded) {
    pthread_mutex_lock(&shutdown_wait_mu);
    while (!shutdown_done) {
      pthread_cond_wait(&shutdown_wait_cv, &shutdown_wait_mu);
    }
    pthread_mutex_unlock(&shutdown_wait_mu);
  }
}

static void mark_shutdown_done() {
  pthread_mutex_lock(&shutdown_wait_mu);
  shutdown_done = 1;
  pthread_cond_broadcast(&shutdown_wait_cv);
  pthread_mutex_unlock(&shutdown_wait_mu);
}

/**
 * Handle a shutdown request from the untrusted command channel.  We should
 * allow user code to override this to do exit processing -- for now,
 * user code will have to use onexit or atexit to do cleanup, which is
 * suboptimal in a multithreaded environment.
 */
static NaClSrpcError srpc_shutdown_request(NaClSrpcChannel* channel,
                                           NaClSrpcArg **in_args,
                                           NaClSrpcArg **out_arg) {
  struct worker_state *state =
      (struct worker_state *) channel->server_instance_data;

  if (state->is_privileged) {
    /*
     * do onexit/atexit processing, then exit_group.  really should do
     * something sane wrt threads, but we do not implement thread
     * cancellation (as yet?).
     */
    mark_shutdown_done();
    exit(0);
  }
  return NACL_SRPC_RESULT_BREAK;
}

/**
 * Export the shutdown method on the main rpc service loop.  Use __shutdown
 * to avoid name collisions.
 * TODO(mseaborn): Remove __shutdown because it is unused.
 */
NACL_SRPC_METHOD("__shutdown::", srpc_shutdown_request);

/**
 * Basic SRPC worker thread: run the NaClSrpcServerLoop.
 */
static void *srpc_worker(void *arg) {
  struct worker_state *state = (struct worker_state *) arg;

  NaClSrpcServerLoop(state->d, __kNaClSrpcHandlers, NULL);

  (void) close(state->d);
  if (state->is_privileged) {
    mark_shutdown_done();
    _exit(0);
  }
  free(arg);
  return 0;
}

/**
 * Acceptor loop: accept client connections, and for each, spawn a
 * worker thread that invokes NaClSrpcServerLoop.
 */
static void *srpc_default_acceptor(void *arg) {
  int       first = (int) arg;
  int       d;

  while (-1 != (d = imc_accept(BOUND_SOCKET))) {
    struct worker_state *state = malloc(sizeof *state);
    pthread_t           worker_tid;

    if (NULL == state) {
      /*
       * shed load; the client can come back later when we have more
       * memory.
       */
      (void) close(d);
      continue;
    }
    state->d = d;
    state->is_privileged = first;
    /* worker thread is responsible for state and d. */
    pthread_create(&worker_tid, NULL, srpc_worker, state);
    first = 0;
  }
  return NULL;
}

/**
 * Internal SRPC initialization.
 *
 * First we check to see if we are running embedded in the browser. If
 * not, we simply return, as there will be no SRPC connections. If
 * embedded we spawn a thread which is our main accept loop. The accept
 * loop spawns worker threads on a per-client-connection basis.
 * (We could use a thread pool, but do not at this point.)  The
 * worker threads just handle RPC requests using NaClSrpcServerLoop().
 * The first worker thread is "privileged", in that it is responsible
 * for shutting down the NaCl app, and we expect that this first
 * connection comes from the browser plugin.
 */
void __srpc_init() {
  pthread_t acceptor_tid;
  int       is_embedded;
  static int init_done = 0;

  /*
   * In context of a browser __srpc_init() is invoked after _init() to make sure
   * all global constructors have finished when we accept the first RPC. Other
   * contexts may require SRPC be initialized before global constructors.
   * Subsequent calls to __srpc_init() must execute as a noop.
   */
  if (init_done) {
    return;
  }
  init_done = 1;

  is_embedded = (srpc_get_fd() != -1);
  if (is_embedded) {
    /*
     * Start the acceptor thread.
     */
    pthread_create(&acceptor_tid, NULL,
                   srpc_default_acceptor, (void *) 1);
    pthread_detach(acceptor_tid);
  }
}
