/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#define BOUND_SOCKET 3

struct WorkerData {
  int sock_fd;
  const struct NaClSrpcHandlerDesc *methods;
};

static void *Worker(void *arg) {
  struct WorkerData *worker_data = (struct WorkerData *) arg;
  NaClSrpcServerLoop(worker_data->sock_fd, worker_data->methods, NULL);
  close(worker_data->sock_fd);
  free(worker_data);
  return NULL;
}

int NaClSrpcAcceptClientOnThread(const struct NaClSrpcHandlerDesc* methods) {
  int sock_fd = -1;
  int result = 0;
  struct WorkerData *worker_data = NULL;
  pthread_t worker_tid;

  sock_fd = imc_accept(BOUND_SOCKET);
  if (sock_fd == -1) {
    goto done;
  }
  worker_data = (struct WorkerData*) malloc(sizeof *worker_data);
  if (worker_data == NULL) {
    goto done;
  }
  worker_data->sock_fd = sock_fd;
  worker_data->methods = methods;
  if (pthread_create(&worker_tid, NULL, Worker, (void *) worker_data) != 0) {
    goto done;
  }
  /* On successful thread start the worker takes ownership. */
  worker_data = NULL;
  sock_fd = -1;
  result = 1;
 done:
  free(worker_data);
  if (sock_fd != -1) {
    close(sock_fd);
  }
  return result;
}
