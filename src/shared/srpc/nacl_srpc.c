/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
#include "native_client/src/shared/srpc/nacl_srpc_message.h"

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
  NaClSrpcArgCtor(&out_carray);
  outs[0] = &out_carray;
  outs[1] = NULL;
  out_carray.tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  out_carray.arrays.carr = NULL;
  /* Set up the basic service descriptor to make the service discovery call. */
  tmp_service = (NaClSrpcService*) malloc(sizeof(*tmp_service));
  if (NULL == tmp_service) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " temporary service malloc failed\n",
                (void*) channel);
    goto cleanup;
  }
  if (!NaClSrpcServiceHandlerCtor(tmp_service, basic_services)) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " NaClSrpcServiceHandlerCtor failed\n",
                (void*) channel);
    goto cleanup;
  }
  /* Channel takes ownership of tmp_service. */
  channel->client = tmp_service;
  tmp_service = NULL;
  /* Build the argument values for invoking service discovery */
  NaClSrpcArgCtor(&out_carray);
  outs[0] = &out_carray;
  outs[1] = NULL;
  out_carray.tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  out_carray.u.count = NACL_SRPC_MAX_SERVICE_DISCOVERY_CHARS;
  out_carray.arrays.carr = calloc(NACL_SRPC_MAX_SERVICE_DISCOVERY_CHARS, 1);
  if (NULL == out_carray.arrays.carr) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " service discovery malloc failed\n",
                (void*) channel);
    goto cleanup;
  }
  /* Invoke service discovery, getting description string */
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeV(channel, 0, ins, outs)) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " service discovery invoke failed\n",
                (void*) channel);
    goto cleanup;
  }
  /* Clean up the temporary service. */
  NaClSrpcServiceDtor(channel->client);
  free(channel->client);
  channel->client = NULL;
  /* Allocate the real service. */
  service = (NaClSrpcService*) malloc(sizeof(*service));
  if (NULL == service) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " service discovery malloc failed\n",
                (void*) channel);
    goto cleanup;
  }
  /* Build the real service from the resulting string. */
  if (!NaClSrpcServiceStringCtor(service, out_carray.arrays.carr)) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "BuildInterfaceDesc(channel=%p):"
                " NaClSrpcServiceStringCtor failed\n",
                (void*) channel);
    goto cleanup;
  }
  channel->client = service;
  /* Free the service string */
  free(out_carray.arrays.carr);
  /* Return success */
  return 1;

 cleanup:
  free(service);
  free(out_carray.arrays.carr);
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
  NaClSrpcLog(1,
              "NaClSrpcClientCtor(channel=%p, handle=%p)\n",
              (void*) channel,
              (void*) handle);
  channel->message_channel = NaClSrpcMessageChannelNew(handle);
  if (NULL == channel->message_channel) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "NaClSrpcClientCtor(channel=%p):"
                " NaClSrpcMessageChannelNew failed\n",
                (void*) channel);
    return 0;
  }
  /* Initialize the server information. */
  channel->server = NULL;
  channel->next_outgoing_request_id = 0;
  /* Do service discovery to speed method invocation. */
  if (!BuildInterfaceDesc(channel)) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "NaClSrpcClientCtor(channel=%p):"
                " BuildInterfaceDesc failed\n",
                (void*) channel);
    NaClSrpcMessageChannelDelete(channel->message_channel);
    channel->message_channel = NULL;
    return 0;
  }
  /* Return success. */
  return 1;
}

int NaClSrpcServerCtor(NaClSrpcChannel* channel,
                       NaClSrpcImcDescType handle,
                       NaClSrpcService* service,
                       void* server_instance_data) {
  NaClSrpcLog(1,
              "NaClSrpcServerCtor(channel=%p, handle=%p,"
              " service=%p, server_instance_data=%p)\n",
              (void*) channel,
              (void*) handle,
              (void*) service,
              (void*) server_instance_data);
  channel->message_channel = NaClSrpcMessageChannelNew(handle);
  if (NULL == channel->message_channel) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "NaClSrpcServerCtor(channel=%p):"
                " NaClSrpcMessageChannelNew failed\n",
                (void*) channel);
    return 0;
  }
  /* Initialize the client information. */
  channel->client = NULL;
  /* Set the service to that passed in. */
  channel->server = service;
  channel->server_instance_data = server_instance_data;
  channel->next_outgoing_request_id = 0;
  /* Return success. */
  return 1;
}

void NaClSrpcDtor(NaClSrpcChannel* channel) {
  NaClSrpcLog(1,
              "NaClSrpcDtor(channel=%p)\n",
              (void*) channel);
  if (NULL == channel) {
    return;
  }
  NaClSrpcServiceDtor(channel->client);
  free(channel->client);
  NaClSrpcServiceDtor(channel->server);
  free(channel->server);
  NaClSrpcMessageChannelDelete(channel->message_channel);
}

void NaClSrpcArgCtor(NaClSrpcArg* arg) {
  memset(arg, 0, sizeof *arg);
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
