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

#include <stdlib.h>
#include <unistd.h>
#include "nacl_srpc.h"
#include "nacl_srpc_internal.h"
#include <sys/nacl_syscalls.h>

#define BOUND_SOCKET  3

/* TODO: This file seems to have relative includes and should probably
   not live in shared/   */

struct worker_state {
  int d;
  int is_privileged;
};

static pthread_mutex_t shutdown_wait_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t shutdown_wait_cv = PTHREAD_COND_INITIALIZER;
static int shutdown_done = 0;

void __srpc_wait() {
  int       is_embedded;

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
static int srpc_shutdown_request(NaClSrpcChannel* channel,
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
 */
NACL_SRPC_METHOD("__shutdown::", srpc_shutdown_request);

/**
 * Basic SRPC worker thread: run the NaClSrpcServerLoop.
 */
static void *srpc_worker(void *arg) {
  struct worker_state *state = (struct worker_state *) arg;

  NaClSrpcServerLoop(state->d, __kNaClSrpcHandlers, (void*) arg);

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
