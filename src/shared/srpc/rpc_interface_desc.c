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
 * Extract and manipulate service descriptions from simple rpc service
 * discovery.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/srpc/rpc_interface_desc.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"


/* TODO(sehr): check for malformed strings. */
NaClSrpcDesc* __NaClSrpcBuildSrpcDesc(const char* str, uint32_t* rpc_count) {
  NaClSrpcDesc* desc;
  const char* p;
  uint32_t i;

  /* Count the number of rpc methods */
  *rpc_count = 0;
  for (p = str; *p != '\0'; ) {
    char* next_p = strchr(p, '\n');
    if (NULL == next_p) {
      /* malformed input -- no remaining \n character was found */
      return NULL;
    }
    p = next_p + 1;
    ++*rpc_count;
  }
  /* Allocate the descriptor array */
  desc = (NaClSrpcDesc*) malloc(*rpc_count * sizeof(*desc));
  if (NULL == desc) {
    *rpc_count = 0;
    return NULL;
  }
  /* Parse the list of method descriptions */
  p = str;
  for (i = 0; i < *rpc_count; ++i) {
    int entry_length;
    char* string;
    char* newline_loc;

    newline_loc = strchr(p, '\n');
    if (NULL == newline_loc) {
      /* malformed input -- no remaining \n character was found */
      return NULL;
    }
    entry_length = newline_loc - p + 1;
    string = (char*) malloc(entry_length);
    if (NULL == string) {
      return NULL;
    }
    /* Parse the name, in_args, out_args string for one method */
    strncpy(string, p, entry_length);
    string[entry_length - 1] = '\0';
    desc[i].out_args = strrchr(string, ':') + 1;
    *strrchr(string, ':') = '\0';
    desc[i].in_args = strrchr(string, ':') + 1;
    *strrchr(string, ':') = '\0';
    desc[i].rpc_name = string;
    p = newline_loc + 1;
    /* The handler will be found through other means. */
    desc[i].handler = NULL;
  }
  return desc;
}

void NaClSrpcDumpInterfaceDesc(const NaClSrpcChannel  *channel) {
  uint32_t i;
  printf("RPC %-20s %-10s %-10s\n", "Name", "Input args", "Output args");
  for (i = 0; i < channel->client.rpc_count; ++i) {
    printf("%3u %-20s %-10s %-10s\n",
           (unsigned int) i,
           channel->client.rpc_descr[i].rpc_name,
           channel->client.rpc_descr[i].in_args,
           channel->client.rpc_descr[i].out_args);
  }
}

int NaClSrpcGetRpcNum(const NaClSrpcChannel  *channel,
                      char const             *name) {
  uint32_t i;
  for (i = 0; i < channel->client.rpc_count;  ++i) {
    if (!strcmp(name, channel->client.rpc_descr[i].rpc_name)) {
      return i;
    }
  }
  return -1;
}
