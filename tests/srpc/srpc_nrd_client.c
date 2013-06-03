/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

/* Flag used to record errors. */
/* It is returned to the JavaScript bridge through the report method. */
static int errors_seen = 0;

/* SockAddrClient implements the NativeClient client portion above. */
void SockAddrClient(NaClSrpcRpc *rpc,
                    NaClSrpcArg **in_args,
                    NaClSrpcArg **out_args,
                    NaClSrpcClosure *done) {
  NaClSrpcChannel channel;
  static char buf[1024];
  size_t buf_size = sizeof(buf);
  int connected_socket;
  int retval;
  int socket_address = in_args[0]->u.hval;
  static char cmp_msg[] = "Quidquid id est, timeo Danaos et dona ferentes";

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
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
    done->Run(done);
    return;
  }
  /* Start the SRPC client to communicate over the connected socket. */
  if (!NaClSrpcClientCtor(&channel, connected_socket)) {
    printf("SockAddrClient: SRPC constructor FAILED\n");
    ++errors_seen;
    done->Run(done);
    return;
  }
  /* Perform an RPC on the SRPC client, which returns a message. */
  if (NACL_SRPC_RESULT_OK
      != NaClSrpcInvokeBySignature(&channel, "getmsg::C", &buf_size, buf)) {
    printf("SockAddrClient: RPC FAILED.\n");
    ++errors_seen;
    done->Run(done);
    return;
  }
  /* Check the message for correctness. */
  printf("SockAddrClient: getmsg returned %s\n", buf);
  if (strcmp(buf, cmp_msg)) {
    printf("SockAddrClient: getmsg return string incorrect\n");
    ++errors_seen;
  }
  /* Shut down the SRPC server and close the client. */
  retval = NaClSrpcInvokeBySignature(&channel, "shutdown::");
  if (NACL_SRPC_RESULT_OK != retval && NACL_SRPC_RESULT_INTERNAL != retval) {
    printf("SockAddrClient: shutdown FAILED, retval = %d.\n", retval);
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
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/* SharedMemoryClient creates and returns a shared memory region. */
void SharedMemoryClient(NaClSrpcRpc *rpc,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args,
                        NaClSrpcClosure *done) {
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
  out_args[1]->arrays.str = strdup(test_string);
  out_args[2]->u.ival = errors_seen;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "sock_addr_client:h:i", SockAddrClient },
  { "shared_memory_client:i:hsi", SharedMemoryClient },
  { NULL, NULL },
};

int main(void) {
  printf("================== CLIENT ================\n");
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  printf("================== CLIENT INIT ================\n");
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    printf("================== CLIENT ACCEPT FAILED ================\n");
    return 1;
  }
  printf("================== CLIENT ACCEPT DONE ================\n");
  NaClSrpcModuleFini();
  return 0;
}
