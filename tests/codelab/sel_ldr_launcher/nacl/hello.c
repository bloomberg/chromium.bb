/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

NaClSrpcError GetGreeting(NaClSrpcChannel *channel,
    NaClSrpcArg **in_args, NaClSrpcArg **out_args) {
  printf("[GetGreeting] GetGreeting is called\n");

  char* name = in_args[0]->u.sval;
  printf("[GetGreeting] name = %s\n", name);

  char* greeting = (char*) calloc(100 + strlen(name), sizeof(char));
  sprintf(greeting, "Hello, %s\n", name);

  /* Note that SRPC layer frees strings passed to it. */
  out_args[0]->u.sval = greeting;

  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("GetGreeting:s:s", GetGreeting);
