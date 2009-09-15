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
