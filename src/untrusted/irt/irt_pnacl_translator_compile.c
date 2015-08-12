/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <argz.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/irt/irt_pnacl_translator_common.h"

static const char kInputVar[] = "NACL_IRT_PNACL_TRANSLATOR_COMPILE_INPUT";
static const char kOutputVar[] = "NACL_IRT_PNACL_TRANSLATOR_COMPILE_OUTPUT";
static const char kArgVar[] = "NACL_IRT_PNACL_TRANSLATOR_COMPILE_ARG";
static const char kThreadsVar[] = "NACL_IRT_PNACL_TRANSLATOR_COMPILE_THREADS";

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
  const char *input_filename = getenv(kInputVar);
  const char *threads_str = getenv(kThreadsVar);
  if (input_filename == NULL && threads_str == NULL) {
    /*
     * TODO(mseaborn): After the PNaCl toolchain revision has been updated,
     * this SRPC case will no longer be used and can be removed.
     */
    g_funcs = funcs;
    if (!NaClSrpcModuleInit()) {
      NaClLog(LOG_FATAL, "NaClSrpcModuleInit() failed\n");
    }
    if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
      NaClLog(LOG_FATAL, "NaClSrpcAcceptClientConnection() failed\n");
    }
    return;
  }

  if (input_filename == NULL) {
    NaClLog(LOG_FATAL, "serve_translate_request: Env var %s not set\n",
            kInputVar);
  }
  if (threads_str == NULL) {
    NaClLog(LOG_FATAL, "serve_translate_request: Env var %s not set\n",
            kThreadsVar);
  }

  /* Open input file. */
  int input_fd = open(input_filename, O_RDONLY);
  if (input_fd < 0) {
    NaClLog(LOG_FATAL,
            "serve_translate_request: Failed to open input file \"%s\": %s\n",
            input_filename, strerror(errno));
  }

  /* Open output files. */
  size_t outputs_count = env_var_prefix_match_count(kOutputVar);
  int output_fds[outputs_count];
  char **env;
  size_t i = 0;
  for (env = environ; *env != NULL; ++env) {
    char *output_filename = env_var_prefix_match(*env, kOutputVar);
    if (output_filename != NULL) {
      int output_fd = creat(output_filename, 0666);
      if (output_fd < 0) {
        NaClLog(LOG_FATAL,
                "serve_translate_request: "
                "Failed to open output file \"%s\": %s\n",
                output_filename, strerror(errno));
      }
      assert(i < outputs_count);
      output_fds[i++] = output_fd;
    }
  }
  assert(i == outputs_count);

  /* Extract list of arguments from the environment variables. */
  size_t args_count = env_var_prefix_match_count(kArgVar);
  char *args[args_count + 1];
  i = 0;
  for (env = environ; *env != NULL; ++env) {
    char *arg = env_var_prefix_match(*env, kArgVar);
    if (arg != NULL) {
      assert(i < args_count);
      args[i++] = arg;
    }
  }
  assert(i == args_count);
  args[i] = NULL;

  int thread_count = atoi(threads_str);
  funcs->init_callback(thread_count, output_fds, outputs_count,
                       args, args_count);
  char buf[0x1000];
  for (;;) {
    ssize_t bytes_read = read(input_fd, buf, sizeof(buf));
    if (bytes_read < 0) {
      NaClLog(LOG_FATAL,
              "serve_translate_request: "
              "Error while reading input file \"%s\": %s\n",
              input_filename, strerror(errno));
    }
    if (bytes_read == 0)
      break;
    funcs->data_callback(buf, bytes_read);
  }
  int rc = close(input_fd);
  if (rc != 0) {
    NaClLog(LOG_FATAL, "serve_translate_request: close() failed: %s\n",
            strerror(errno));
  }
  funcs->end_callback();
}

const struct nacl_irt_private_pnacl_translator_compile
    nacl_irt_private_pnacl_translator_compile = {
  serve_translate_request
};
