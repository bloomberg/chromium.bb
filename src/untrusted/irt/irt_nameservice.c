/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/name_service.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


/*
 * Lock to guard name service channel.  Both the local data structure and
 * the channel itself can only be used for one request at a time.  So we
 * serialize requests.  We could revisit this and do something that scales
 * to multithreaded use better if there's demand for that later.
 */
static pthread_mutex_t name_service_mutex = PTHREAD_MUTEX_INITIALIZER;
static int ns_channel_initialized = 0;
static struct NaClSrpcChannel ns_channel;

static int prepare_nameservice(void) {
  int ns = -1;
  int connected_socket;
  int error;

  if (ns_channel_initialized)
    return 0;
  nacl_nameservice(&ns);
  if (-1 == ns) {
    error = errno;
    dprintf(2, "IRT: nacl_nameservice failed: %s\n", strerror(error));
    return error;
  }

  connected_socket = imc_connect(ns);
  error = errno;
  close(ns);
  if (-1 == connected_socket) {
    dprintf(2, "IRT: imc_connect to nameservice failed: %s\n", strerror(error));
    return error;
  }

  if (!NaClSrpcClientCtor(&ns_channel, connected_socket)) {
    dprintf(2, "IRT: NaClSrpcClientCtor failed for nameservice\n");
    return EIO;
  }

  ns_channel_initialized = 1;
  return 0;
}

/*
 * Look up a name in the nameservice.  Returns a NACL_NAME_SERVICE_* code
 * or -1 for an SRPC error.  On returning NACL_NAME_SERVICE_SUCCESS,
 * *out_fd has the descriptor for the service found.
 */
int irt_nameservice_lookup(const char *name, int oflag, int *out_fd) {
  int error;
  int status = -1;

  pthread_mutex_lock(&name_service_mutex);

  error = prepare_nameservice();
  if (0 == error) {
    if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(
            &ns_channel, NACL_NAME_SERVICE_LOOKUP, name, oflag,
            &status, out_fd)) {
      dprintf(2, "IRT: SRPC failure for NACL_NAME_SERVICE_LOOKUP: %s\n",
              strerror(errno));
    }
  }

  pthread_mutex_unlock(&name_service_mutex);

  return status;
}
