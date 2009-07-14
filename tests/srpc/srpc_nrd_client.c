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
 * NativeClient client side of a test for passing socket addresses.  The
 * client side steps are:
 * 1) The JavaScript bridge passes the "start" method socket address.
 * 2) Start then connects to that socket address, giving a connected socket.
 * 3) Start creates an SRPC client bound to the connected socket.
 * 4) Start performs an RPC on the SRPC client, which returns a message.
 * 5) Start compares the message for correctness.
 */

#include <errno.h>
#include <nacl/nacl_srpc.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>

/* Flag used to record errors. */
/* It is returned to the JavaScript bridge through the report method. */
static int errors_seen = 0;

/* SockAddrClient implements the NativeClient client portion described above. */
int SockAddrClient(NaClSrpcChannel *old_channel,
                   NaClSrpcArg **in_args,
                   NaClSrpcArg **out_args);

NACL_SRPC_METHOD("sock_addr_client:h:i", SockAddrClient);

int SockAddrClient(NaClSrpcChannel *old_channel,
                   NaClSrpcArg **in_args,
                   NaClSrpcArg **out_args) {
  NaClSrpcChannel channel;
  static char buf[1024];
  int connected_socket;
  int socket_address = in_args[0]->u.hval;
  static char cmp_msg[] = "Quidquid id est, timeo Danaos et dona ferentes";

  /* Start up banner, clear errors. */
  printf("SockAddrClient: socket address is %d\n", socket_address);
  errors_seen = 0;
  /* Connect to the socket address passed in, giving a new connected socket. */
  connected_socket = imc_connect(socket_address);
  if (connected_socket >= 0) {
    printf("SockAddrClient: connected as %d\n", connected_socket);
  } else {
    printf("SockAddrClient: connect FAILED, errno %d\n", errno);
    ++errors_seen;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Start the SRPC client to communicate over the connected socket. */
  if (!NaClSrpcClientCtor(&channel, connected_socket)) {
    printf("SockAddrClient: SRPC constructor FAILED\n");
    ++errors_seen;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Perform an RPC on the SRPC client, which returns a message. */
  if (NACL_SRPC_RESULT_OK
      != NaClSrpcInvokeByName(&channel, "getmsg", sizeof(buf), buf)) {
    printf("SockAddrClient: RPC FAILED.\n");
    ++errors_seen;
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Check the message for correctness. */
  printf("SockAddrClient: getmsg returned %s\n", buf);
  if (strcmp(buf, cmp_msg)) {
    printf("SockAddrClient: getmsg return string incorrect\n");
    ++errors_seen;
  }
  /* Shut down the SRPC server and close the client. */
  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeByName(&channel, "shutdown")) {
    printf("SockAddrClient: shutdown FAILED.\n");
    ++errors_seen;
  }
  printf("SockAddrClient: shutting down, errors_seen %d\n", errors_seen);
  NaClSrpcDtor(&channel);
  /* Close the connected socket and the socket address descriptor. */
  if (0 != close(connected_socket)) {
    printf("SockAddrClient: connected socket close failed\n");
    ++errors_seen;
  }
  if (0 != close(socket_address)) {
    printf("SockAddrClient: socket address close failed\n");
    ++errors_seen;
  }
  /* Return the error count and success. */
  out_args[0]->u.ival = errors_seen;
  return NACL_SRPC_RESULT_OK;
}

/* SharedMemoryClient creates and returns a shared memory region. */
int SharedMemoryClient(NaClSrpcChannel *channel,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args);

NACL_SRPC_METHOD("shared_memory_client:i:hsi", SharedMemoryClient);

int SharedMemoryClient(NaClSrpcChannel *channel,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args) {
  int desc;
  char* test_string = "salvete, omnes";
  size_t size = in_args[0]->u.ival;
  /* Start up banner, clear errors. */
  printf("SharedMemoryClient: size %d\n", size);
  errors_seen = 0;
  /* Create an IMC shared memory region. */
  desc = imc_mem_obj_create(size);
  if (desc >= 0) {
    char* map_addr;
    map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
    if (MAP_FAILED == map_addr) {
      printf("SharedMemoryClient: mmap failed\n");
      ++errors_seen;
    } else {
      strncpy(map_addr, test_string, strlen(test_string));
    }
  } else {
    printf("SharedMemoryClient: imc_mem_obj_create failed %d\n", desc);
    ++errors_seen;
  }
  /* Return the imc descriptor, the string we wrote, and the error count. */
  out_args[0]->u.hval = desc;
  /* NOTE: string return values from SRPC need to be heap allocated. */
  out_args[1]->u.sval = strdup(test_string);
  out_args[2]->u.ival = errors_seen;
  return NACL_SRPC_RESULT_OK;
}
