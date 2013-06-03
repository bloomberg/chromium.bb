/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Test for simple rpc based access to name services.
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


int EnumerateNames(struct StringBuffer *sb, NaClSrpcChannel *nschan) {
  char      buffer[1024];
  uint32_t  nbytes = sizeof buffer;
  char      *p;
  size_t    name_len;

  if (NACL_SRPC_RESULT_OK != NaClSrpcInvokeBySignature(nschan,
                                                       NACL_NAME_SERVICE_LIST,
                                                       &nbytes, buffer)) {
    return 0;
  }
  StringBufferPrintf(sb, "nbytes = %zu\n", (size_t) nbytes);
  if (nbytes == sizeof buffer) {
    fprintf(stderr, "Insufficent space for namespace enumeration\n");
    return 0;
  }
  for (p = buffer; p - buffer < nbytes; p += name_len) {
    name_len = strlen(p) + 1;
    StringBufferPrintf(sb, "%s\n", p);
  }
  return 1;
}

/*
 * Return name service output
 */
void NameServiceDump(NaClSrpcRpc *rpc,
                     NaClSrpcArg **in_args,
                     NaClSrpcArg **out_args,
                     NaClSrpcClosure *done) {
  struct StringBuffer sb;
  StringBufferCtor(&sb);

  if (EnumerateNames(&sb, &ns_channel)) {
    out_args[0]->arrays.str = strdup(sb.buffer);
    rpc->result = NACL_SRPC_RESULT_OK;
  } else {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  }
  done->Run(done);
  StringBufferDtor(&sb);
}


void ManifestTest(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  struct StringBuffer sb;
  int                 status;
  int                 manifest;
  char                buffer[1024];
  uint32_t            nbytes = sizeof buffer;
  char                *p;
  size_t              name_len;

  /* just get the descriptor for now */
  StringBufferCtor(&sb);
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest)) {
    fprintf(stderr, "nameservice lookup RPC failed\n");
  }
  StringBufferPrintf(&sb, "Got manifest descriptor %d\n", manifest);
  if (-1 == manifest) {
    fprintf(stderr, "nameservice lookup failed: status %d\n", status);
  } else {
    /* connect to manifest name server */
    int manifest_conn;
    struct NaClSrpcChannel manifest_channel;

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
    if (NACL_SRPC_RESULT_OK !=
        NaClSrpcInvokeBySignature(&manifest_channel, NACL_NAME_SERVICE_LIST,
                                  &nbytes, buffer)) {
      StringBufferPrintf(&sb, "manifest list failed\n");
      goto done;
    }
    StringBufferDiscardOutput(&sb);
    StringBufferPrintf(&sb, "Manifest Contents:\n");
    for (p = buffer; p - buffer < nbytes; p += name_len + 1) {
      name_len = strlen(p);
      StringBufferPrintf(&sb, "%.*s\n", (int) name_len, p);
    }
  }
 done:
  out_args[0]->arrays.str = strdup(sb.buffer);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
  StringBufferDtor(&sb);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  /* Export the methods as taking no arguments and returning a string. */
  { "namedump::s", NameServiceDump },
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
