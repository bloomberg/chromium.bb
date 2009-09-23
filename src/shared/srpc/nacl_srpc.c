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


/*
 * NaCl service library.  a primitive rpc library
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef __native_client__
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#endif  /* __native_client__ */
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

/*
 * Service discovery is used to build an interface description that
 * is searched for rpc dispatches.
 */
static int BuildInterfaceDesc(NaClSrpcChannel* channel) {
  NaClSrpcArg*         ins[] = { NULL };
  NaClSrpcArg          out_carray;
  NaClSrpcArg*         outs[2];
  NaClSrpcService*     tmp_service = NULL;
  NaClSrpcService*     service = NULL;
  NaClSrpcHandlerDesc  basic_services[] = { { NULL, NULL } };

  /* Set up the output parameters for service discovery. */
  outs[0] = &out_carray;
  outs[1] = NULL;
  out_carray.tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  out_carray.u.caval.carr = NULL;
  /* Set up the basic service descriptor to make the service discovery call. */
  tmp_service = (NaClSrpcService*) malloc(sizeof(*tmp_service));
  if (NULL == tmp_service) {
    goto cleanup;
  }
  if (!NaClSrpcServiceHandlerCtor(tmp_service, basic_services)) {
    goto cleanup;
  }
  channel->client = tmp_service;
  /* Build the argument values for invoking service discovery */
  outs[0] = &out_carray;
  outs[1] = NULL;
  out_carray.tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  out_carray.u.caval.count = NACL_SRPC_MAX_SERVICE_DISCOVERY_CHARS;
  out_carray.u.caval.carr = calloc(NACL_SRPC_MAX_SERVICE_DISCOVERY_CHARS, 1);
  if (NULL == out_carray.u.caval.carr) {
    goto cleanup;
  }
  /* Invoke service discovery, getting description string */
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeV(channel, 0, ins, outs)) {
    goto cleanup;
  }
  /* Clean up the temporary service. */
  NaClSrpcServiceDtor(tmp_service);
  free(tmp_service);
  tmp_service = NULL;
  /* Allocate the real service. */
  service = (NaClSrpcService*) malloc(sizeof(*service));
  if (NULL == service) {
    goto cleanup;
  }
  /* Build the real service from the resulting string. */
  if (!NaClSrpcServiceStringCtor(service, out_carray.u.caval.carr)) {
    goto cleanup;
  }
  channel->client = service;
  /* Free the service string */
  free(out_carray.u.caval.carr);
  /* Return success */
  return 1;

 cleanup:
  free(service);
  free(out_carray.u.caval.carr);
  NaClSrpcServiceDtor(tmp_service);
  free(tmp_service);
  return 0;
}

/*
 * The constructors and destructor.
 */

/*
 * Set up the buffering structures for a channel.
 */
int NaClSrpcClientCtor(NaClSrpcChannel* channel, NaClSrpcImcDescType handle) {
  /* TODO(sehr): we should actually do a NaClDescRef here. */
  channel->imc_handle = handle;

#ifndef __native_client__
  if (NULL == channel->imc_handle) {
    return 0;
  }
  if (0 == NaClNrdXferEffectorCtor(&channel->eff, channel->imc_handle)) {
    return 0;
  }
#endif
  /* Initialize the server information. */
  channel->server = NULL;
  /* Construct the buffers. */
  __NaClSrpcImcBufferCtor(&channel->send_buf, 1);
  __NaClSrpcImcBufferCtor(&channel->receive_buf, 0);
  /* Disable timing and initialize the timing counters. */
  channel->timing_enabled = 0;
  channel->send_usec = 0.0;
  channel->receive_usec = 0.0;
  channel->imc_read_usec = 0.0;
  channel->imc_write_usec = 0.0;
  channel->next_outgoing_request_id = 0;
  /* Do service discovery to speed method invocation. */
  if (!BuildInterfaceDesc(channel)) {
    return 0;
  }
  /* Return success. */
  return 1;
}

int NaClSrpcServerCtor(NaClSrpcChannel* channel,
                       NaClSrpcImcDescType handle,
                       NaClSrpcService* service,
                       void* server_instance_data) {
  /* TODO(sehr): we should actually do a NaClDescRef here. */
  channel->imc_handle = handle;
#ifndef __native_client__
  if (NULL == channel->imc_handle) {
    return 0;
  }
  if (0 == NaClNrdXferEffectorCtor(&channel->eff, channel->imc_handle)) {
    return 0;
  }
#endif
  /* Initialize the client information. */
  channel->client = NULL;
  /* Construct the buffers. */
  __NaClSrpcImcBufferCtor(&channel->send_buf, 1);
  __NaClSrpcImcBufferCtor(&channel->receive_buf, 0);
  /* Set the service to that passed in. */
  channel->server = service;
  /* Disable timing and initialize the timing counters. */
  channel->timing_enabled = 0;
  channel->send_usec = 0.0;
  channel->receive_usec = 0.0;
  channel->imc_read_usec = 0.0;
  channel->imc_write_usec = 0.0;
  channel->server_instance_data = server_instance_data;
  channel->next_outgoing_request_id = 0;
  /* Return success. */
  return 1;
}

void NaClSrpcDtor(NaClSrpcChannel *channel) {
  if (NULL == channel) {
    return;
  }
#ifndef __native_client__
  /* SCOPE */ {
    struct NaClDescEffector* effp = (struct NaClDescEffector*) &channel->eff;
    effp->vtbl->Dtor(effp);
    NaClDescUnref(channel->imc_handle);
  }
#endif
  NaClSrpcServiceDtor(channel->client);
  free(channel->client);
  NaClSrpcServiceDtor(channel->server);
  free(channel->server);
}

/*
 * Channel timing support.
 */
void NaClSrpcToggleChannelTiming(NaClSrpcChannel* channel, int enable_timing) {
  channel->timing_enabled = enable_timing;
}

void NaClSrpcGetTimes(NaClSrpcChannel* channel,
                      double* send_time,
                      double* receive_time,
                      double* imc_read_time,
                      double* imc_write_time) {
  *send_time = channel->send_usec;
  *receive_time = channel->receive_usec;
  *imc_read_time = channel->imc_read_usec;
  *imc_write_time = channel->imc_write_usec;
}
