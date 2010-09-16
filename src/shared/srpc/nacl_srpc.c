/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#if defined(__native_client__)
  channel->imc_handle = handle;
#else
  if (NULL == handle) {
    return 0;
  }
  if (0 == NaClNrdXferEffectorCtor(&channel->eff)) {
    return 0;
  }
  channel->imc_handle = NaClDescRef(handle);
#endif  /* defined(__native_client__) */
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
#if defined(__native_client__)
  channel->imc_handle = handle;
#else
  if (NULL == handle) {
    return 0;
  }
  if (0 == NaClNrdXferEffectorCtor(&channel->eff)) {
    return 0;
  }
  channel->imc_handle = NaClDescRef(handle);
#endif  /* defined(__native_client__) */
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

/*
 * A standalone SRPC server is not a subprocess of the browser or
 * sel_universal.  As this is a mode used for testing, the parent environment
 * must set the following variable to indicate that fact.
 */
const char kSrpcStandalone[] = "NACL_SRPC_STANDALONE";

int NaClSrpcIsStandalone() {
  return (NULL != getenv(kSrpcStandalone));
}
