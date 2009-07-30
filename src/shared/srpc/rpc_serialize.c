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
 * NaCl simple RPC over IMC mechanism.
 */

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#ifdef __native_client__
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#else
#include "native_client/src/include/portability.h"
#endif

#ifndef SIZE_T_MAX
# define SIZE_T_MAX (~((size_t) 0))
#endif

/*
 * Message formats:
 * SRPC communicates using two main message types, requests and responses.
 * Both are encoded with variable sizes, according to the table.
 *
 * Request
 *   protocol               - 4 bytes
 *   rpc method index       - 4 bytes
 *   #args                  - 4 bytes
 *   #args * (arg value)    - varying size
 *   #rets                  - 4 bytes
 *   #rets * (arg template) - varying size
 *
 * Response
 *   protocol               - 4 bytes
 *   return code            - 4 bytes
 *   #rets                  - 4 bytes
 *   #rets * (arg value)    - varying size
 *
 * Sub-messages:
 *
 * arg value:
 *   type               - 1 byte
 *   union
 *     bool             - 1 byte
 *     int              - 4 bytes
 *     double           - 8 bytes
 *     string
 *       length         - 4 bytes
 *       value          - (length) * 1 bytes ('\0' is not sent)
 *     char array
 *       length         - 4 bytes
 *       value          - (length) * 1 bytes
 *     int array
 *       length         - 4 bytes
 *       value          - (length) * 4 bytes
 *     double array
 *       length         - 4 bytes
 *       value          - (length) * 8 bytes
 *     descriptor handle
 *       (work in progress)
 *
 * arg template:
 *   type               - 1 byte
 *   union
 *     bool             - 0 bytes
 *     int              - 0 bytes
 *     double           - 0 bytes
 *     string           - 0 bytes
 *     char array
 *       length         - 4 bytes
 *     int array
 *       length         - 4 bytes
 *     double array
 *       length         - 4 bytes
 *     descriptor handle
 *       (work in progress)
 */


/*
 * Support for timing the SRPC infrastructure.
 */
static NaClSrpcError GetTimes(NaClSrpcChannel* channel,
                              NaClSrpcArg** in_args,
                              NaClSrpcArg** out_args);
static NaClSrpcError SetTimingEnabled(NaClSrpcChannel* channel,
                                      NaClSrpcArg** in_args,
                                      NaClSrpcArg** out_args);


/*
 *  The high level APIs provided for external use.
 */
NaClSrpcError NaClSrpcReceiveAndDispatch(NaClSrpcChannel* channel) {
  NaClSrpcArg*    args[NACL_SRPC_MAX_ARGS+1];
  NaClSrpcArg*    rets[NACL_SRPC_MAX_ARGS+1];
  const char*     in_types;
  const char*     out_types;
  uint32_t        rpc_number;
  NaClSrpcError   retval;
  NaClSrpcError   app_error;
  int             return_break = 0;
  double          this_start_usec = 0.0;
  double          this_method_usec;

  /*
   * If we are timing, get the start time.
   */
  if (channel->timing_enabled) {
    this_start_usec = __NaClSrpcGetUsec();
  }

  /*
   * First we receive the parameters for the method
   */
  dprintf(("SERVER: receiving RPC request\n"));
  retval = __NaClSrpcImcReadRequestHeader(channel, (unsigned int*) &rpc_number);
  if (retval != 1) {
    dprintf(("SERVER:     RPC header read failed: %d\n", retval));
    return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
  }
  dprintf(("SERVER:     rpc number %u\n", (unsigned) rpc_number));
  if (rpc_number == NACL_SRPC_GET_TIMES_METHOD) {
    in_types = "";
    out_types = "dddd";
  } else if (rpc_number == NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD) {
    in_types = "";
    out_types = "i";
  } else if (rpc_number >= channel->rpc_count) {
    dprintf(("SERVER:     RPC bad rpc number: %u not in [0, %u)\n",
             (unsigned) rpc_number, (unsigned) channel->rpc_count));
    return NACL_SRPC_RESULT_BAD_RPC_NUMBER;
  } else {
    in_types = channel->rpc_descr[rpc_number].in_args;
    out_types = channel->rpc_descr[rpc_number].out_args;
  }
  retval = __NaClSrpcArgsGet(channel, 1, 1, args, in_types);
  if (NACL_SRPC_RESULT_OK != retval) {
    dprintf(("SERVER:     argument vector receive failed: %d\n", retval));
    return retval;
  }
  dprintf(("SERVER:     argument vector received\n"));
  retval = __NaClSrpcArgsGet(channel, 1, 0, rets, out_types);
  if (NACL_SRPC_RESULT_OK != retval) {
    dprintf(("SERVER:     return descriptor receive failed: %d, %s\n", retval,
             NaClSrpcErrorString(retval)));
    return retval;
  }
  dprintf(("SERVER: received RPC request\n"));

  /*
   * Then we invoke the method, which computes a return code.
   */
  dprintf(("SERVER: invoking RPC %u\n", (unsigned) rpc_number));
  if (NACL_SRPC_GET_TIMES_METHOD == rpc_number) {
    app_error = GetTimes(channel, args, rets);
  } else if (NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD == rpc_number) {
    app_error = SetTimingEnabled(channel, args, rets);
  } else {
    app_error = (channel->rpc_descr[rpc_number].handler)(channel, args, rets);
    if (NACL_SRPC_RESULT_BREAK == app_error) {
      return_break = 1;
      app_error = NACL_SRPC_RESULT_OK;
    }
  }
  dprintf(("SERVER: performed RPC\n"));

  /*
   * Then we return the response
   */
  __NaClSrpcImcWriteResponseHeader(channel, app_error);
  dprintf(("SERVER:     wrote header: app_error %d\n", app_error));
  if ((retval = __NaClSrpcArgsPut(channel, 1, rets)) != NACL_SRPC_RESULT_OK) {
    dprintf(("SERVER:     return value(s) send failed: %d\n", retval));
    return retval;
  }
  dprintf(("SERVER:     return value(s) sent\n"));
  retval = __NaClSrpcImcFlush(channel);
  if (1 != retval) {
    /*
     * If the ArgsPut call fails due to a bad handle, the transport layer
     * doesn't send anything.  Therefore we need to return an error message
     * so that the client doesn't wait forever.
     */
    __NaClSrpcImcWriteResponseHeader(channel, NACL_SRPC_RESULT_INTERNAL);
    __NaClSrpcImcFlush(channel);
  }
  dprintf(("SERVER: sent response\n"));

  /*
   * Then we free any memory that might have been allocated for array-typed
   * arguments or returns.
   */
  __NaClSrpcArgsFree(args);
  __NaClSrpcArgsFree(rets);
  dprintf(("SERVER: freed memory\n"));

  /*
   * If we are timing, collect the current time, compute the delta from
   * the start, and update the cumulative counter.
   */
  if (channel->timing_enabled) {
    this_method_usec = __NaClSrpcGetUsec();
    channel->receive_usec += this_method_usec;
  }

  if (return_break) {
    return NACL_SRPC_RESULT_BREAK;
  } else {
    return NACL_SRPC_RESULT_OK;
  }
}

void __NaClSrpcArgsFree(NaClSrpcArg* argvec[]) {
  NaClSrpcArg** argvecp;

  for (argvecp = argvec; *argvecp != NULL; ++argvecp) {
    switch ((*argvecp)->tag) {
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free((*argvecp)->u.caval.carr);
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free((*argvecp)->u.daval.darr);
      break;
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free((*argvecp)->u.iaval.iarr);
      break;
     case NACL_SRPC_ARG_TYPE_INVALID:
     case NACL_SRPC_ARG_TYPE_BOOL:
     case NACL_SRPC_ARG_TYPE_DOUBLE:
     case NACL_SRPC_ARG_TYPE_HANDLE:
     case NACL_SRPC_ARG_TYPE_INT:
      break;
     case NACL_SRPC_ARG_TYPE_STRING:
      free((*argvecp)->u.sval);
      break;
     /*
      * The two cases below are added to avoid warnings, they are only used
      * in the plugin code
      */
     case NACL_SRPC_ARG_TYPE_OBJECT:
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
     default:
      break;
    }
  }
  free(argvec[0]);
}

char* NaClSrpcErrorString(NaClSrpcError error_code) {
  switch (error_code) {
   case NACL_SRPC_RESULT_OK:
     return "No error";
   case NACL_SRPC_RESULT_BREAK:
     return "Break out of server RPC loop";
   case NACL_SRPC_RESULT_MESSAGE_TRUNCATED:
     return "Received message was shorter than expected";
   case NACL_SRPC_RESULT_NO_MEMORY:
     return "Out of memory";
   case NACL_SRPC_RESULT_PROTOCOL_MISMATCH:
     return "Client and server have different protocol versions";
   case NACL_SRPC_RESULT_BAD_RPC_NUMBER:
     return "No method for the given rpc number";
   case NACL_SRPC_RESULT_BAD_ARG_TYPE:
     return "Bad argument type received";
   case NACL_SRPC_RESULT_TOO_MANY_ARGS:
     return "Too many arguments (more than NACL_SRPC_MAX_ARGS or declared)";
   case NACL_SRPC_RESULT_TOO_FEW_ARGS:
     return "Too few arguments (fewer than declared)";
   case NACL_SRPC_RESULT_IN_ARG_TYPE_MISMATCH:
     return "Input argument type mismatch";
   case NACL_SRPC_RESULT_OUT_ARG_TYPE_MISMATCH:
     return "Output argument type mismatch";
   case NACL_SRPC_RESULT_INTERNAL:
     return "Internal error in rpc method";
   case NACL_SRPC_RESULT_APP_ERROR:
     return "Rpc application returned an error";
  }
  return "Unrecognized NaClSrpcError value";
}

/*
 * Static function definitions.
 */
NaClSrpcError __NaClSrpcArgsGet(NaClSrpcChannel* channel,
                                int allocate_args,
                                int read_values,
                                NaClSrpcArg* argvec[],
                                const char* arg_types) {
  uint32_t dim;
  size_t length;
  NaClSrpcArg *args;
  int retval = NACL_SRPC_RESULT_OK;
  size_t i;

  retval = __NaClSrpcImcRead(&length, sizeof(length), 1, channel);
  if (retval != 1) {
    return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
  }
  if (length >= NACL_SRPC_MAX_ARGS) {
    /*
     * It is an error if the argument length exceeds the length of the
     * vector passed in.
     */
    return NACL_SRPC_RESULT_TOO_MANY_ARGS;
  }

  dprintf(("GET: length %"PRIdS"\n", length));

  if (allocate_args && length > 0) {
    size_t ix;

    if (length > SIZE_T_MAX / sizeof(NaClSrpcArg)) {
      retval = NACL_SRPC_RESULT_NO_MEMORY;
      goto error;
    }
    /*
     * post condition: no integer overflow, so
     * length * sizeof(NaClSrpcArg) <= SIZE_T_MAX
     */
    args = (NaClSrpcArg*) malloc(length * sizeof(NaClSrpcArg));
    if (args == NULL) {
      retval = NACL_SRPC_RESULT_NO_MEMORY;
      goto error;
    }
    memset((void*) args, 0, length * sizeof(NaClSrpcArg));

    /*
     * Initialize the arg type tags with those specified in the declaration.
     */
    for (ix = 0; ix < length; ++ix) {
      if (arg_types[ix] == ':' || arg_types[ix] == '\0') {
        return NACL_SRPC_RESULT_TOO_MANY_ARGS;
      }
      args[ix].tag = arg_types[ix];
    }
    /* TODO(sehr): include test code to validate arglist mismatches */
    if (arg_types[length] != ':' && arg_types[length] != '\0') {
      return NACL_SRPC_RESULT_TOO_FEW_ARGS;
    }
  } else {
    args = argvec[0];
  }

  for (i = 0; i < length; ++i) {
    char read_type;

    retval = __NaClSrpcImcRead(&read_type, sizeof(char), 1, channel);
    if (retval != 1) {
      retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
      goto error;
    }
    if (args[i].tag != read_type) {
      dprintf(("arg[%"PRIdS"]: tag %d, expected %d\n", i, read_type,
               args[i].tag));
      retval = NACL_SRPC_RESULT_BAD_ARG_TYPE;
      goto error;
    }

    argvec[i] = args + i;

    switch (args[i].tag) {
     case NACL_SRPC_ARG_TYPE_INVALID:
      retval = NACL_SRPC_RESULT_BAD_ARG_TYPE;
      goto error;
     case NACL_SRPC_ARG_TYPE_BOOL:
      if (read_values) {
        retval = __NaClSrpcImcRead(&args[i].u.bval, sizeof(char), 1, channel);
        if (retval != 1) {
          retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
          goto error;
        }
      }
      dprintf(("value %d\n", args[i].u.bval));
      break;
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      retval = __NaClSrpcImcRead(&dim, sizeof(dim), 1, channel);
      if (retval != 1) {
        retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        goto error;
      }
      dprintf(("dim %"PRIu32"\n", dim));
      if (allocate_args) {
        /* sizeof(char) == 1, so no overflow possible below */
        args[i].u.caval.carr = (char*) malloc(dim * sizeof(char));
        if (args[i].u.caval.carr == NULL) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.caval.count = dim;
      } else {
        if (args[i].u.caval.count < dim) {
          return NACL_SRPC_RESULT_TOO_MANY_ARGS;
        }
      }
      if (read_values) {
        retval = __NaClSrpcImcRead(args[i].u.caval.carr, 1, dim, channel);
        if (retval != dim) {
          return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        }
      }
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE:
      if (read_values) {
        retval = __NaClSrpcImcRead(&args[i].u.dval, sizeof(double), 1, channel);
        if (retval != 1) {
          return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        }
      }
      dprintf(("value %f\n", args[i].u.dval));
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      retval = __NaClSrpcImcRead(&dim, sizeof(dim), 1, channel);
      if (retval != 1) {
        retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        goto error;
      }
      dprintf(("dim %"PRIu32"\n", dim));
      if (allocate_args) {
        if (dim > SIZE_T_MAX / sizeof(double)) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.daval.darr = (double*) malloc(dim * sizeof(double));
        if (args[i].u.daval.darr == NULL) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.daval.count = dim;
      } else {
        if (args[i].u.daval.count < dim) {
          return NACL_SRPC_RESULT_TOO_MANY_ARGS;
        }
      }
      if (read_values) {
        retval = __NaClSrpcImcRead(args[i].u.daval.darr,
                                   sizeof(double),
                                   dim,
                                   channel);
        if (retval != dim) {
          return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        }
      }
      break;
     case NACL_SRPC_ARG_TYPE_HANDLE:
      if (read_values) {
        args[i].u.hval = __NaClSrpcImcReadDesc(channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_INT:
      if (read_values) {
        retval = __NaClSrpcImcRead(&args[i].u.ival, sizeof(int), 1, channel);
        if (retval != 1) {
          return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        }
      }
      dprintf(("value %d\n", args[i].u.ival));
      break;
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      retval = __NaClSrpcImcRead(&dim, sizeof(dim), 1, channel);
      if (retval != 1) {
        retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        goto error;
      }
      dprintf(("dim %u\n", (unsigned) dim));
      if (allocate_args) {
        if (dim > SIZE_T_MAX / sizeof(int)) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.iaval.iarr = (int*) malloc(dim * sizeof(int));
        if (args[i].u.iaval.iarr == NULL) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.iaval.count = dim;
      } else {
        if (args[i].u.iaval.count < dim) {
          return NACL_SRPC_RESULT_TOO_MANY_ARGS;
        }
      }
      if (read_values) {
        retval = __NaClSrpcImcRead(args[i].u.iaval.iarr,
                                   sizeof(int),
                                   dim,
                                   channel);
        if (retval != dim) {
          return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
        }
      }
      break;
     case NACL_SRPC_ARG_TYPE_STRING:
      if (read_values) {
        retval = __NaClSrpcImcRead(&dim, sizeof(dim), 1, channel);
        if (retval != 1) {
          retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
          goto error;
        }
        /*
         * check if dim + 1 (in the malloc below) will result in an
         * integer overflow
         */
        if (dim > SIZE_T_MAX - 1) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        args[i].u.sval = (char*) malloc(dim + 1);
        if (args[i].u.sval == NULL) {
          retval = NACL_SRPC_RESULT_NO_MEMORY;
          goto error;
        }
        retval = __NaClSrpcImcRead(args[i].u.sval, sizeof(char), dim, channel);
        if (retval != dim) {
          retval = NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
          goto error;
        }
        args[i].u.sval[dim] = '\0';
      }
      break;
     /*
      * The two cases below are added to avoid warnings, they are only used
      * in the plugin code
      */
     case NACL_SRPC_ARG_TYPE_OBJECT:
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
     default:
      retval = NACL_SRPC_RESULT_BAD_ARG_TYPE;
      goto error;
    }
  }
  argvec[length] = NULL;
  return NACL_SRPC_RESULT_OK;
error:
  if (args != NULL) {
    __NaClSrpcArgsFree(argvec);
  }
  return retval;
}

NaClSrpcError __NaClSrpcArgsPut(NaClSrpcChannel* channel,
                                int write_values,
                                NaClSrpcArg* argvec[]) {
  int i;
  int length;
  size_t slen;
  NaClSrpcArg* arg;

  for (length = 0; argvec[length] != NULL; ++length) {}
  if (length >= NACL_SRPC_MAX_ARGS) {
    /*
     * It is an error if the argument length exceeds the length of the
     * vector passed in.
     */
    return NACL_SRPC_RESULT_TOO_MANY_ARGS;
  }
  __NaClSrpcImcWrite(&length, sizeof(length), 1, channel);
  dprintf(("PUT: length %d\n", length));

  for (i = 0; i < length; ++i) {
    arg = argvec[i];
    __NaClSrpcImcWrite(&arg->tag, sizeof(char), 1, channel);

    dprintf(("PUT:     arg[%d]: tag %d, ", i, arg->tag));

    switch (arg->tag) {
     case NACL_SRPC_ARG_TYPE_INVALID:
      break;
     case NACL_SRPC_ARG_TYPE_BOOL:
      dprintf(("value %d\n", arg->u.bval));
      if (write_values) {
        __NaClSrpcImcWrite(&arg->u.bval, sizeof(char), 1, channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      dprintf(("dim %u\n", (unsigned) arg->u.caval.count));
      __NaClSrpcImcWrite(&arg->u.caval.count, sizeof(uint32_t), 1, channel);
      dprintf(("PUT:     sent length, carr = %p\n", arg->u.caval.carr));
      if (write_values) {
        __NaClSrpcImcWrite(arg->u.caval.carr, 1, arg->u.caval.count, channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE:
      dprintf(("value %f\n", arg->u.dval));
      if (write_values) {
        __NaClSrpcImcWrite(&arg->u.dval, sizeof(double), 1, channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      dprintf(("dim %u\n", (unsigned) arg->u.daval.count));
      __NaClSrpcImcWrite(&arg->u.daval.count, sizeof(uint32_t), 1, channel);
      dprintf(("PUT:     sent length, darr = %p\n", (void*) arg->u.daval.darr));
      if (write_values) {
        __NaClSrpcImcWrite(arg->u.daval.darr,
                 sizeof(double),
                 arg->u.daval.count,
                 channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_HANDLE:
      /* TODO: temporarily disabled because of compiler warning */
#if 0
      dprintf(("value %"PRIxPTR"\n", (uintptr_t) arg->u.hval));
#endif
      if (write_values) {
        __NaClSrpcImcWriteDesc(channel, arg->u.hval);
      }
      break;
     case NACL_SRPC_ARG_TYPE_INT:
      dprintf(("value %d\n", arg->u.ival));
      if (write_values) {
        __NaClSrpcImcWrite(&arg->u.ival, sizeof(int), 1, channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      dprintf(("dim %u\n", (unsigned) arg->u.iaval.count));
      __NaClSrpcImcWrite(&arg->u.iaval.count, sizeof(uint32_t), 1, channel);
      dprintf(("PUT:     sent length, iarr = %p\n", (void*) arg->u.iaval.iarr));
      if (write_values) {
        __NaClSrpcImcWrite(arg->u.iaval.iarr,
                           sizeof(int),
                           arg->u.iaval.count,
                           channel);
      }
      break;
     case NACL_SRPC_ARG_TYPE_STRING:
      if (write_values) {
        slen = strlen(arg->u.sval);
        dprintf(("strlen %u\n", (unsigned) slen));
        __NaClSrpcImcWrite(&slen, sizeof(slen), 1, channel);
        __NaClSrpcImcWrite(arg->u.sval, 1, strlen(arg->u.sval), channel);
      }
      break;
     /*
      * The two cases below are added to avoid warnings, they are only used
      * in the plugin code
      */
     case NACL_SRPC_ARG_TYPE_OBJECT:
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
     default:
      break;
    }
    dprintf(("PUT:     sent value\n"));
  }
  return NACL_SRPC_RESULT_OK;
}

static NaClSrpcError GetTimes(NaClSrpcChannel* channel,
                              NaClSrpcArg** in_args,
                              NaClSrpcArg** out_args) {
  NaClSrpcGetTimes(channel,
                   &out_args[0]->u.dval,
                   &out_args[1]->u.dval,
                   &out_args[2]->u.dval,
                   &out_args[3]->u.dval);
  return NACL_SRPC_RESULT_OK;
}

static NaClSrpcError SetTimingEnabled(NaClSrpcChannel* channel,
                                      NaClSrpcArg** in_args,
                                      NaClSrpcArg** out_args) {
  NaClSrpcToggleChannelTiming(channel, in_args[0]->u.ival);
  return NACL_SRPC_RESULT_OK;
}

void NaClSrpcToggleChannelTiming(NaClSrpcChannel* channel, int enable_timing) {
  channel->timing_enabled = enable_timing;
}

void NaClSrpcGetTimes(NaClSrpcChannel* channel,
                      double* send_time,
                      double* receive_time,
                      double* imc_read_time,
                      double* imc_write_time) {
  *send_time = channel->send_usec;
  *receive_time = channel->receive_usec;
  *imc_read_time = channel->imc_read_usec;
  *imc_write_time = channel->imc_write_usec;
}
