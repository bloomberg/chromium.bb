/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"

static const char kInputsVar[] = "NACL_IRT_PNACL_TRANSLATOR_LINK_INPUTS";
static const char kOutputVar[] = "NACL_IRT_PNACL_TRANSLATOR_LINK_OUTPUT";

static const int kMaxObjectFiles = 16;

static int (*g_func)(int nexe_fd,
                     const int *obj_file_fds,
                     int obj_file_fd_count);

static void handle_link_request(NaClSrpcRpc *rpc,
                                NaClSrpcArg **in_args,
                                NaClSrpcArg **out_args,
                                NaClSrpcClosure *done) {
  int obj_file_count = in_args[0]->u.ival;
  int nexe_fd = in_args[kMaxObjectFiles + 1]->u.hval;

  if (obj_file_count < 1 || obj_file_count > kMaxObjectFiles) {
    NaClLog(LOG_FATAL, "Bad object file count (%i)\n", obj_file_count);
  }
  int obj_file_fds[obj_file_count];
  for (int i = 0; i < obj_file_count; i++) {
    obj_file_fds[i] = in_args[i + 1]->u.hval;
  }

  int result = g_func(nexe_fd, obj_file_fds, obj_file_count);

  rpc->result = result == 0 ? NACL_SRPC_RESULT_OK : NACL_SRPC_RESULT_APP_ERROR;
  done->Run(done);
}

static const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "RunWithSplit:ihhhhhhhhhhhhhhhhh:", handle_link_request },
  { NULL, NULL },
};

static size_t count_colon_separators(const char *str) {
  size_t count = 0;
  for (; *str != '\0'; ++str) {
    if (*str == ':')
      ++count;
  }
  return count;
}

static void serve_link_request(int (*func)(int nexe_fd,
                                           const int *obj_file_fds,
                                           int obj_file_fd_count)) {
  const char *inputs_str = getenv(kInputsVar);
  const char *output_filename = getenv(kOutputVar);
  if (inputs_str == NULL && output_filename == NULL) {
    /*
     * TODO(mseaborn): After the PNaCl toolchain revision has been updated,
     * this SRPC case will no longer be used and can be removed.
     */
    g_func = func;
    if (!NaClSrpcModuleInit()) {
      NaClLog(LOG_FATAL, "NaClSrpcModuleInit() failed\n");
    }
    if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
      NaClLog(LOG_FATAL, "NaClSrpcAcceptClientConnection() failed\n");
    }
    return;
  }

  if (inputs_str == NULL)
    NaClLog(LOG_FATAL, "serve_link_request: Env var %s not set\n", kInputsVar);
  if (output_filename == NULL)
    NaClLog(LOG_FATAL, "serve_link_request: Env var %s not set\n", kOutputVar);

  /* Open output file. */
  int nexe_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (nexe_fd < 0) {
    NaClLog(LOG_FATAL,
            "serve_link_request: Failed to open output file \"%s\": %s\n",
            output_filename, strerror(errno));
  }

  /* Open input files.  Parse colon-separated list. */
  if (*inputs_str == '\0')
    NaClLog(LOG_FATAL, "serve_link_request: List of inputs is empty\n");
  char *inputs_copy = strdup(inputs_str);
  size_t inputs_count = 1 + count_colon_separators(inputs_copy);
  int input_fds[inputs_count];
  char *pos = inputs_copy;
  size_t i = 0;
  while (pos != NULL) {
    char *input_filename = pos;
    strsep(&pos, ":");
    int input_fd = open(input_filename, O_RDONLY);
    if (input_fd < 0) {
      NaClLog(LOG_FATAL,
              "serve_link_request: Failed to open input file \"%s\": %s\n",
              input_filename, strerror(errno));
    }
    assert(i < inputs_count);
    input_fds[i++] = input_fd;
  }
  assert(i == inputs_count);
  free(inputs_copy);

  func(nexe_fd, input_fds, inputs_count);
}

const struct nacl_irt_private_pnacl_translator_link
    nacl_irt_private_pnacl_translator_link = {
  serve_link_request,
};
