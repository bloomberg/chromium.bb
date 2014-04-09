/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * SRPC-based test for simple rpc based access to name services.
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

struct StringBuffer {
  size_t  nbytes;
  size_t  insert;
  char    *buffer;
};

NaClSrpcChannel ns_channel;

void StringBufferCtor(struct StringBuffer *sb) {
  sb->nbytes = 1024;
  sb->insert = 0;
  sb->buffer = malloc(sb->nbytes);
  if (NULL == sb->buffer) {
    perror("StringBufferInit::malloc");
    abort();
  }
  sb->buffer[0] = '\0';
}

void StringBufferDiscardOutput(struct StringBuffer *sb) {
  sb->insert = 0;
  sb->buffer[0] = '\0';
}

void StringBufferDtor(struct StringBuffer *sb) {
  sb->nbytes = 0;
  sb->insert = 0;
  free(sb->buffer);
  sb->buffer = NULL;
}

void StringBufferPrintf(struct StringBuffer *sb,
                        char const *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void StringBufferPrintf(struct StringBuffer *sb,
                        char const *fmt, ...) {
  size_t space;
  char *insert_pt;
  va_list ap;
  int written = 0;
  char *new_buffer;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  for (;;) {
    space = sb->nbytes - sb->insert;
    insert_pt = sb->buffer + sb->insert;
    va_start(ap, fmt);
    written = vsnprintf(insert_pt, space, fmt, ap);
    va_end(ap);
    if (written < space) {
      sb->insert += written;
      break;
    }
    /* insufficient space -- grow the buffer */
    new_buffer = realloc(sb->buffer, 2 * sb->nbytes);
    if (NULL == new_buffer) {
      /* give up */
      fprintf(stderr, "StringBufferPrintf: no memory\n");
      break;
    }
    sb->nbytes *= 2;
    sb->buffer = new_buffer;
  }
}

void dump_output(struct StringBuffer *sb, int d, size_t nbytes) {
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
      StringBufferPrintf(sb, "\n%04x:", ix);
    } else if (0 == (ix & (BYTE_SPACING-1))) {
      StringBufferPrintf(sb, " ");
    }
    StringBufferPrintf(sb, "%02x", bytes[ix]);
  }
  StringBufferPrintf(sb, "\n");

  free(bytes);
}

/*
 * Dump RNG output into a string.
 */
void RngDump(NaClSrpcRpc *rpc,
             NaClSrpcArg **in_args,
             NaClSrpcArg **out_args,
             NaClSrpcClosure *done) {
  struct StringBuffer sb;
  NaClSrpcError rpc_result;
  int status;
  int rng;

  StringBufferCtor(&sb);
  rpc_result = NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                         "SecureRandom", O_RDONLY,
                                         &status, &rng);
  assert(NACL_SRPC_RESULT_OK == rpc_result);
  printf("rpc status %d\n", status);
  assert(NACL_NAME_SERVICE_SUCCESS == status);
  printf("rng descriptor %d\n", rng);

  dump_output(&sb, rng, RNG_OUTPUT_BYTES);
  out_args[0]->arrays.str = strdup(sb.buffer);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
  close(rng);
  StringBufferDtor(&sb);
}

void ManifestTest(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  struct StringBuffer     sb;
  NaClSrpcError           rpc_result;
  int                     status;
  int                     manifest;
  int                     manifest_conn;
  struct NaClSrpcChannel  manifest_channel;

  /* just get the descriptor for now */
  StringBufferCtor(&sb);
  if (NACL_SRPC_RESULT_OK !=
      (rpc_result =
       NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                 "ManifestNameService", O_RDWR,
                                 &status, &manifest))) {
    StringBufferPrintf(&sb, "nameservice lookup RPC failed (%d)\n", rpc_result);
    goto done;
  }
  StringBufferPrintf(&sb, "Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    goto done;
  }
  /* connect to manifest name server */

  manifest_conn = imc_connect(manifest);
  StringBufferPrintf(&sb, "got manifest connection %d\n", manifest_conn);
  if (-1 == manifest_conn) {
    StringBufferPrintf(&sb, "could not connect\n");
    goto done;
  }
  close(manifest);
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    StringBufferPrintf(&sb, "could not build srpc client\n");
    goto done;
  }
  StringBufferDiscardOutput(&sb);
  StringBufferPrintf(&sb, "ManifestTest: basic connectivity ok\n");

 done:
  out_args[0]->arrays.str = strdup(sb.buffer);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
  StringBufferDtor(&sb);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  /* Export the methods as taking no arguments and returning a string. */
  { "rngdump::s", RngDump },
  { "manifest_test::s", ManifestTest },
  { NULL, NULL },
};

int main(void) {
  int ns;
  int connected_socket;

  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  ns = -1;
  nacl_nameservice(&ns);
  assert(-1 != ns);

  connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&ns_channel, connected_socket)) {
    fprintf(stderr, "Srpc client channel ctor failed\n");
    return 1;
  }
  close(ns);

  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
