/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"

#include <argz.h>
#include <string.h>

static const int kMaxObjectFiles = 16;

static const struct nacl_irt_pnacl_compile_funcs *g_funcs;

static void stream_init_with_split(
    NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
    NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  int num_threads = in_args[0]->u.ival;
  if (num_threads < 0 || num_threads > kMaxObjectFiles) {
    NaClLog(LOG_FATAL, "Invalid # of threads (%d)\n", num_threads);
  }
  int fd_start = 1;
  int i = fd_start;
  int num_valid_fds = 0;
  int obj_fds[kMaxObjectFiles];
  while (num_valid_fds < kMaxObjectFiles && in_args[i]->u.hval >= 0) {
    obj_fds[num_valid_fds] = in_args[i]->u.hval;
    ++i;
    ++num_valid_fds;
  }
  /*
   * Convert the null-delimited strings into an array of
   * null-terminated strings.
   */
  char *cmd_argz = in_args[kMaxObjectFiles + fd_start]->arrays.carr;
  size_t cmd_argz_len = in_args[kMaxObjectFiles + fd_start]->u.count;
  size_t argc = argz_count(cmd_argz, cmd_argz_len);
  char **argv = (char **)malloc((argc + 1) * sizeof(char *));
  argz_extract(cmd_argz, cmd_argz_len, argv);
  char *result = g_funcs->init_callback(num_threads, obj_fds, num_valid_fds,
                                        argv, argc);
  free(argv);
  if (result != NULL) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    /*
     * SRPC wants to free() the string, so just strdup here so that the
     * init_callback implementation doesn't have to know if the string
     * comes from malloc or new.  On error, we don't care so much
     * about leaking this bit of memory.
     */
    out_args[0]->arrays.str = strdup(result);
  } else {
    rpc->result = NACL_SRPC_RESULT_OK;
    out_args[0]->arrays.str = strdup("");
  }
  done->Run(done);
}

static void stream_chunk(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  int result = g_funcs->data_callback(
      in_args[0]->arrays.carr,
      in_args[0]->u.count);
  rpc->result = result == 0 ? NACL_SRPC_RESULT_OK : NACL_SRPC_RESULT_APP_ERROR;
  done->Run(done);
}

static void stream_end(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  char *result = g_funcs->end_callback();
  /* Fill in the deprecated return values with dummy values. */
  out_args[0]->u.ival = 0;
  out_args[1]->arrays.str = strdup("");
  out_args[2]->arrays.str = strdup("");
  if (result != NULL) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    /* SRPC wants to free(), so strdup() and leak to hide this detail. */
    out_args[3]->arrays.str = strdup(result);
  } else {
    rpc->result = NACL_SRPC_RESULT_OK;
    out_args[3]->arrays.str = strdup("");
  }
  done->Run(done);
}

static const struct NaClSrpcHandlerDesc srpc_methods[] = {
  /*
   * Protocol for streaming:
   * StreamInitWithSplit(num_threads, obj_fd x 16, cmdline_flags) -> error_str
   * StreamChunk(data) +
   * TODO(jvoung): remove these is_shared_lib, etc.
   * StreamEnd() -> (is_shared_lib, soname, dependencies, error_str)
   */
  { "StreamInitWithSplit:ihhhhhhhhhhhhhhhhC:s", stream_init_with_split },
  { "StreamChunk:C:", stream_chunk },
  { "StreamEnd::isss", stream_end },
  { NULL, NULL },
};

static void serve_translate_request(
    const struct nacl_irt_pnacl_compile_funcs *funcs) {
  g_funcs = funcs;
  if (!NaClSrpcModuleInit()) {
    NaClLog(LOG_FATAL, "NaClSrpcModuleInit() failed\n");
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    NaClLog(LOG_FATAL, "NaClSrpcAcceptClientConnection() failed\n");
  }
}

const struct nacl_irt_private_pnacl_translator_compile
    nacl_irt_private_pnacl_translator_compile = {
  serve_translate_request
};
