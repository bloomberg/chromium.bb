/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

/*
 * ServerLoop processes the RPCs that this descriptor listens to.
 */
static int ServerLoop(NaClSrpcService* service,
                      NaClSrpcImcDescType socket_desc,
                      void* instance_data) {
  NaClSrpcChannel* channel = NULL;
  int retval = 0;

  /*
   * SrpcChannel objects can be large due to the buffers they contain.
   * Hence they should not be allocated on a thread stack.
   */
  channel = (NaClSrpcChannel*) malloc(sizeof(*channel));
  if (NULL == channel) {
    goto cleanup;
  }
  /* Create an SRPC server listening on the new file descriptor. */
  if (!NaClSrpcServerCtor(channel, socket_desc, service, instance_data)) {
    goto cleanup;
  }
  /*
   * Loop receiving RPCs and processing them.
   * The loop stops when a method requests a break out of the loop
   * or the IMC layer is unable to satisfy a request.
   */
  NaClSrpcRpcWait(channel, NULL);
  retval = 1;

 cleanup:
  NaClSrpcDtor(channel);
  free(channel);
  return retval;
}

int NaClSrpcServerLoop(NaClSrpcImcDescType imc_socket_desc,
                       const NaClSrpcHandlerDesc methods[],
                       void* instance_data) {
  NaClSrpcService* service;

  /* Ensure we are passed a valid socket descriptor. */
#ifdef __native_client__
  if (imc_socket_desc <= 0) {
    return 0;
  }
#else
  if (NULL == imc_socket_desc) {
    return 0;
  }
#endif
  /* Allocate the service structure. */
  service = (struct NaClSrpcService*) malloc(sizeof(*service));
  if (NULL == service) {
    return 0;
  }
  /* Build the service descriptor. */
  if (!NaClSrpcServiceHandlerCtor(service, methods)) {
    free(service);
    return 0;
  }
  /* Process the RPCs.  ServerLoop takes ownership of service. */
  return ServerLoop(service, imc_socket_desc, instance_data);
}
