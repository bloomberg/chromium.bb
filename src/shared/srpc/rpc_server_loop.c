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
 * Each server rpc loop keeps its own copy of the descriptors.
 */
struct ServiceDesc {
  NaClSrpcImcDescType         socket_desc;
  struct NaClSrpcHandlerDesc* methods;
  uint32_t                    method_count;
  void*                       server_instance_data;
};

/*
 * The started SRPC server exports this service_discovery method.
 */
static NaClSrpcError ServiceDiscovery(NaClSrpcChannel* channel,
                                      NaClSrpcArg** in_args,
                                      NaClSrpcArg** out_args) {
  if (out_args[0]->u.caval.count >= channel->service_string_length) {
    strncpy(out_args[0]->u.caval.carr,
            channel->service_string,
            channel->service_string_length);
    return NACL_SRPC_RESULT_OK;
  } else {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
}

/*
 * The method tables passed to construction do not contain "intrinsic" methods
 * such as service discovery and shutdown.  Build a complete table including
 * those from a given input.
 */
struct NaClSrpcHandlerDesc* __NaClSrpcCompleteMethodTable(
    const struct NaClSrpcHandlerDesc methods[],
    uint32_t* method_count) {
  static const char* kSDDescString = "service_discovery::C";
  struct NaClSrpcHandlerDesc* complete_methods;
  uint32_t i;

  /* Compute the number of methods to export. */
  *method_count = 0;
  while (NULL != methods[*method_count].entry_fmt)
    ++*method_count;
  /* Add one extra method for service discovery. */
  ++*method_count;
  /* Allocate the method descriptors. One extra for NULL termination. */
  complete_methods = (struct NaClSrpcHandlerDesc*)
      malloc((*method_count + 1) * sizeof(*complete_methods));
  if (NULL == complete_methods) {
    return NULL;
  }
  /* Copy the methods passed in, adding service discovery as element zero. */
  complete_methods[0].entry_fmt = kSDDescString;
  complete_methods[0].handler = ServiceDiscovery;
  for (i = 0; i < *method_count - 1; ++i) {
    complete_methods[i + 1].entry_fmt = methods[i].entry_fmt;
    complete_methods[i + 1].handler = methods[i].handler;
  }
  /* Add the NULL terminator */
  complete_methods[*method_count].entry_fmt = NULL;
  complete_methods[*method_count].handler = NULL;
  /* Return the array */
  return complete_methods;
}

char* __NaClSrpcBuildSDString(const struct NaClSrpcHandlerDesc methods[],
                              uint32_t method_count,
                              size_t *length) {
  uint32_t i;
  char* p;
  char* str;

  /* Find the total length of the method description strings.  */
  *length = 1;
  for (i = 0; i < method_count; ++i) {
    *length += strlen(methods[i].entry_fmt) + 1;
  }
  /* Allocate the string. */
  str = (char*) malloc(*length + 1);
  if (NULL == str) {
    return NULL;
  }
  /* Concatenate the method description strings into the string. */
  p = str;
  for (i = 0; i < method_count; ++i) {
    size_t buf_limit = str + *length - p;
    /* TODO(robertm): explore use portability_io.h */
#if NACL_WINDOWS
    p += _snprintf(p, buf_limit, "%s\n", methods[i].entry_fmt);
#else
    p += snprintf(p, buf_limit, "%s\n", methods[i].entry_fmt);
#endif
  }
  *p = '\0';
  /* Return the resulting string. */
  return str;
}

static struct ServiceDesc* BuildDesc(
    const struct NaClSrpcHandlerDesc methods[],
    NaClSrpcImcDescType socket_desc,
    void* server_instance_data,
    char** service_str,
    size_t* service_str_length) {
  struct ServiceDesc* service_desc;

  /*
   * Allocate the service description structure.
   */
  service_desc = (struct ServiceDesc*) malloc(sizeof(*service_desc));
  if (NULL == service_desc) {
    return NULL;
  }
  /* Remember the instance specific data. */
  service_desc->server_instance_data = server_instance_data;
  /* Fill in the socket descriptor. */
  service_desc->socket_desc = socket_desc;
  /* Build the complete method table. */
  service_desc->methods =
      __NaClSrpcCompleteMethodTable(methods, &service_desc->method_count);
  if (NULL == service_desc->methods) {
    return NULL;
  }
  /* Build the service discovery string */
  *service_str = __NaClSrpcBuildSDString(service_desc->methods,
                                         service_desc->method_count,
                                         service_str_length);
  /* Return the completed descriptor. */
  return service_desc;
}

/*
 * ServerLoop processes the RPCs that this descriptor listens to.
 */
static int ServerLoop(struct ServiceDesc* desc,
                      const char* service_str,
                      size_t service_str_length) {
  int retval;
  NaClSrpcChannel* channel;

  /*
   * SrpcChannel objects can be large due to the buffers they contain.
   * Hence they should not be allocated on a thread stack.
   */
  channel = (NaClSrpcChannel*) malloc(sizeof(*channel));
  if (NULL == channel) {
    printf("Could not allocate SRPC channel structure.\n");
    return 0;
  }
  /*
   * Create an SRPC server listening on the new file descriptor.
   */
  if (!NaClSrpcServerCtor(channel,
                          desc->socket_desc,
                          desc->methods,
                          desc->server_instance_data)) {
    return 0;
  }
  /*
   * Save the service discovery string and length to avoid recomputation.
   */
  channel->service_string = service_str;
  channel->service_string_length = service_str_length;
  /*
   * Loop receiving RPCs and processing them.
   * The loop stops when a method requests a break out of the loop
   * or the IMC layer is unable to satisfy a request.
   */
  do {
    retval = NaClSrpcReceiveAndDispatch(channel);
  } while ((NACL_SRPC_RESULT_BREAK != retval) &&
           (NACL_SRPC_RESULT_MESSAGE_TRUNCATED != retval));
  /*
   * Disconnect the SRPC channel.
   */
  NaClSrpcDtor(channel);
  /*
   * Free the NaClSrpcChannel structure
   */
  free(channel);
  /*
   * Return success.
   */
  return 1;
}

int NaClSrpcServerLoopImcDesc(NaClSrpcImcDescType imc_socket_desc,
                              const NaClSrpcHandlerDesc methods[],
                              void* server_instance_data) {
  struct ServiceDesc* service_desc;
  char* service_str;
  size_t service_str_length;

  /*
   * Ensure we are passed a valid socket descriptor.
   */
#ifdef __native_client__
  if (imc_socket_desc <= 0) {
    printf("socket descriptor %d invalid\n", imc_socket_desc);
    return 0;
  }
#else
  if (NULL == imc_socket_desc) {
    printf("socket descriptor invalid\n");
    return 0;
  }
#endif
  /*
   * Build a descriptor to be used to dispatch RPCs.
   */
  service_desc = BuildDesc(methods,
                           imc_socket_desc,
                           server_instance_data,
                           &service_str,
                           &service_str_length);
  if (NULL == service_desc) {
    printf("service descriptor NULL\n");
    return 0;
  }
  /*
   * Process the RPCs.
   */
  return ServerLoop(service_desc, service_str, service_str_length);
}

int NaClSrpcServerLoop(NaClHandle socket_desc,
                       const NaClSrpcHandlerDesc methods[],
                       void* server_instance_data) {
  /*
   * Ensure we are passed a valid socket descriptor.
   */
  if (socket_desc <= 0) {
    printf("socket descriptor %d invalid\n", socket_desc);
    return 0;
  }
  return NaClSrpcServerLoopImcDesc(NaClSrpcImcDescTypeFromHandle(socket_desc),
                                   methods, server_instance_data);
}

#ifdef __native_client__

/* TODO: move this function out of shared/ */
#include <sys/nacl_syscalls.h>

int NaClSrpcDefaultServerLoop(void* server_instance_data) {
  return NaClSrpcServerLoop(srpc_get_fd(),
                            __kNaClSrpcHandlers,
                            server_instance_data);
}
#endif  /* __native_client__ */
