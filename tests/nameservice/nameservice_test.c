/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/name_service.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#define RNG_OUTPUT_BYTES  1024

#define BYTES_PER_LINE    32
#define BYTE_SPACING      4

void dump_output(int d, size_t nbytes) {
  uint8_t *bytes;
  int     got;
  int     copied;
  int     ix;

  bytes = malloc(nbytes);
  if (!bytes) {
    perror("dump_output");
    fprintf(stderr, "No memory\n");
    return;
  }
  /* read output */
  for (got = 0; got < nbytes; got += copied) {
    copied = read(d, bytes + got, nbytes - got);
    if (-1 == copied) {
      perror("dump_output:read");
      fprintf(stderr, "read failure\n");
      break;
    }
    printf("read(%d, ..., %zd) -> %d\n", d, nbytes - got, copied);
  }
  /* hex dump it */
  for (ix = 0; ix < got; ++ix) {
    if (0 == (ix & (BYTES_PER_LINE-1))) {
      printf("\n%04x:", ix);
    } else if (0 == (ix & (BYTE_SPACING-1))) {
      putchar(' ');
    }
    printf("%02x", bytes[ix]);
  }
  putchar('\n');

  free(bytes);
}

int EnumerateNames(NaClSrpcChannel *nschan) {
  char      buffer[1024];
  uint32_t  nbytes = sizeof buffer;
  char      *p;
  size_t    name_len;

  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(nschan,
                                                       NACL_NAME_SERVICE_LIST,
                                                       &nbytes, buffer)) {
    return 0;
  }
  printf("nbytes = %"PRIu32"\n", nbytes);
  if (nbytes == sizeof buffer) {
    fprintf(stderr, "Insufficent space for namespace enumeration\n");
    return 0;
  }
  for (p = buffer; p - buffer < nbytes; p += name_len) {
    name_len = strlen(p) + 1;
    printf("%s\n", p);
  }
  return 1;
}

int main(void) {
  int ns;
  NaClSrpcChannel channel;
  int connected_socket;
  int status;
  int rng;

  if (!NaClSrpcModuleInit()) {
    fprintf(stderr, "srpc module init failed\n");
    return 1;
  }
  printf("Hello world\n");
  ns = -1;
  nacl_nameservice(&ns);
  printf("ns = %d\n", ns);
  assert(-1 != ns);

  connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&channel, connected_socket)) {
    fprintf(stderr, "Srpc client channel ctor failed\n");
    return 1;
  }
  printf("NaClSrpcClientCtor succeeded\n");
  if (!EnumerateNames(&channel)) {
    fprintf(stderr, "Could not enumerate names\n");
    return 1;
  }
  printf("EnumerateNames succeeded\n");
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&channel, NACL_NAME_SERVICE_LOOKUP,
                                "SecureRandom", O_RDONLY, &status, &rng)) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    return 1;
  }
  printf("rpc status %d\n", status);
  assert(NACL_NAME_SERVICE_SUCCESS == status);
  printf("rng descriptor %d\n", rng);

  dump_output(rng, RNG_OUTPUT_BYTES);

  return 0;
}
