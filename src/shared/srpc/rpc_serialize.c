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
 * Both are communicated with an rpc (header) prepended.
 *
 * rpc:
 *   protocol               - (uint32_t) 4 bytes
 *   message id             - (uint64_t) 8 bytes
 *   request/response       - (uint8_t) 1 byte
 *   rpc method index       - (uint32_t) 4 bytes
 *   return code            - (uint32_t) 4 bytes (only sent for responses)
 *
 * request:
 *   #args                  - (uint32_t) 4 bytes
 *   #args * (arg value)    - varying size defined by interface below
 *   #rets                  - (uint32_t) 4 bytes
 *   #rets * (arg template) - varying size defined by interface below
 *
 * response:
 *   #rets                  - (uint32_t) 4 bytes
 *   #rets * (arg value)    - varying size defined by interface below
 *
 */

/*
 * Elements of argument lists are serialized using a type-specific interface.
 * ArgEltDesc contains function pointers for basic operations on the argument
 * list elements.
 */
typedef struct ArgEltInterface ArgEltInterface;
struct ArgEltInterface {
  int (*get)(NaClSrpcChannel* channel,
             int allocate,
             int read_value,
             NaClSrpcArg* arg);
  int (*put)(NaClSrpcArg* arg, int write_value, NaClSrpcChannel* channel);
  void (*print)(NaClSrpcArg* arg);
  uint32_t (*length)(NaClSrpcArg* arg, int write_value, int* descs);
  void (*free)(NaClSrpcArg* arg);
};

/*
 * Argument lists are serialized using a protocol version specific interface.
 */
typedef struct ArgsIoInterface ArgsIoInterface;
struct ArgsIoInterface {
  int (*get)(const ArgsIoInterface* argsdesc,
             NaClSrpcChannel* channel,
             int allocate_args,
             int read_values,
             NaClSrpcArg* argvec[],
             const char* arg_types);
  int (*put)(const ArgsIoInterface* argsdesc,
             NaClSrpcChannel* channel,
             int write_value,
             NaClSrpcArg* argvec[]);
  int (*length)(const ArgsIoInterface* argsdesc,
                NaClSrpcArg* argvec[],
                int write_value,
                uint32_t* bytes,
                uint32_t* handles);
  void (*free)(const ArgsIoInterface* argsdesc,
               NaClSrpcArg* argvec[]);
  const ArgEltInterface* (*element_interface)(NaClSrpcArg* arg);
};
static const ArgsIoInterface* GetArgsInterface(uint32_t protocol_version);

/*
 * Support for timing the SRPC infrastructure.
 */
static NaClSrpcError GetTimes(NaClSrpcChannel* channel,
                              NaClSrpcArg** in_args,
                              NaClSrpcArg** out_args);
static NaClSrpcError SetTimingEnabled(NaClSrpcChannel* channel,
                                      NaClSrpcArg** in_args,
                                      NaClSrpcArg** out_args);

static int NaClSrpcGetArgTypes(NaClSrpcChannel* channel,
                               uint32_t rpc_number,
                               const char **rpc_name,
                               const char **in_types,
                               const char **out_types) {
  if (NACL_SRPC_GET_TIMES_METHOD == rpc_number) {
    *rpc_name = "NACL_SRPC_GET_TIMES_METHOD";
    *in_types = "";
    *out_types = "dddd";
  } else if (NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD == rpc_number) {
    *rpc_name = "NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD";
    *in_types = "";
    *out_types = "i";
  } else if (rpc_number >= channel->rpc_count) {
    dprintf(("SERVER:     RPC bad rpc number: %u not in [0, %u)\n",
             (unsigned) rpc_number, (unsigned) channel->rpc_count));
    return 0;
  } else {
    *rpc_name = channel->rpc_descr[rpc_number].rpc_name;
    *in_types = channel->rpc_descr[rpc_number].in_args;
    *out_types = channel->rpc_descr[rpc_number].out_args;
  }
  return 1;
}

static NaClSrpcMethod GetMethod(NaClSrpcChannel* channel,
                                uint32_t rpc_number) {
  if (NACL_SRPC_GET_TIMES_METHOD == rpc_number) {
    return GetTimes;
  } else if (NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD == rpc_number) {
    return SetTimingEnabled;
  } else if (rpc_number >= channel->rpc_count) {
    return NULL;
  } else {
    return channel->rpc_descr[rpc_number].handler;
  }
}

/*
 *  The high level APIs provided for external use.
 */
NaClSrpcError NaClSrpcReceiveAndDispatch(NaClSrpcChannel* channel) {
  NaClSrpcRpc rpc;
  NaClSrpcArg* args[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg* rets[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcMethod method;
  NaClSrpcError retval;
  int return_break = 0;
  double this_start_usec = 0.0;
  double this_method_usec;
  const ArgsIoInterface* desc;

  /* If we are timing, get the start time. */
  if (channel->timing_enabled) {
    this_start_usec = __NaClSrpcGetUsec();
  }
  /* Read a message from the channel. */
  if (!NaClSrpcRequestRead(channel, &rpc, args, rets)) {
    dprintf((SIDE "ReceiveAndDispatch: receive message failed\n"));
    return NACL_SRPC_RESULT_MESSAGE_TRUNCATED;
  }
  desc = GetArgsInterface(rpc.protocol_version);
  /* Then we invoke the method, which computes a return code. */
  method = GetMethod(channel, rpc.rpc_number);
  if (NULL == method) {
    dprintf((SIDE "ReceiveAndDispatch: bad rpc number %"PRIu32"\n",
             rpc.rpc_number));
    return NACL_SRPC_RESULT_BAD_RPC_NUMBER;
  }
  rpc.app_error = (*method)(channel, args, rets);
  if (NACL_SRPC_RESULT_BREAK == rpc.app_error) {
    dprintf((SIDE "ReceiveAndDispatch: method returned break\n"));
    return_break = 1;
    rpc.app_error = NACL_SRPC_RESULT_OK;
  }
  dprintf((SIDE "ReceiveAndDispatch: invoked method\n"));
  /* Then we return the rets. */
  retval = NaClSrpcResponseWrite(channel, &rpc, rets);
  /* Then we free the memory for the args and rets. */
  desc->free(desc, args);
  desc->free(desc, rets);
  if (NACL_SRPC_RESULT_OK != retval) {
    /* If the response write failed, there was a serious error.  Return. */
    return retval;
  }
  dprintf((SIDE "ReceiveAndDispatch: destroyed args and rets\n"));
  /*
   * If we are timing, collect the current time, compute the delta from
   * the start, and update the cumulative counter.
   */
  if (channel->timing_enabled) {
    this_method_usec = __NaClSrpcGetUsec();
    channel->receive_usec += this_method_usec;
  }
  /* Return code to either continue or break out of the processing loop. */
  if (return_break) {
    return NACL_SRPC_RESULT_BREAK;
  } else {
    return NACL_SRPC_RESULT_OK;
  }
}

/*
 * Static function definitions.
 */
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


/*
 * Argument element I/O interfaces.
 */

#define BASIC_TYPE_IO_DECLARE(name, impl_type, field, format)   \
static int name##Get(NaClSrpcChannel* channel,                  \
                     int allocate_memory,                       \
                     int read_value,                            \
                     NaClSrpcArg* arg) {                        \
  if (read_value &&                                             \
      1 != __NaClSrpcImcRead(&arg->u.field,                     \
                             sizeof(impl_type),                 \
                             1,                                 \
                             channel)) {                        \
    return 0;                                                   \
  }                                                             \
  return 1;                                                     \
}                                                               \
                                                                \
static int name##Put(NaClSrpcArg* arg,                          \
                     int write_value,                           \
                     NaClSrpcChannel* channel) {                \
  if (write_value) {                                            \
    return 1 == __NaClSrpcImcWrite(&arg->u.field,               \
                                   sizeof(impl_type),           \
                                   1,                           \
                                   channel);                    \
  }                                                             \
  return 1;                                                     \
}                                                               \
                                                                \
static void name##Print(NaClSrpcArg* arg) {                     \
  dprintf(("%"#format"", arg->u.field));                        \
}                                                               \
                                                                \
static uint32_t name##Length(NaClSrpcArg* arg,                  \
                             int write_value,                   \
                             int* handles) {                    \
  *handles = 0;                                                 \
  if (write_value) {                                            \
    return sizeof(impl_type);                                   \
  } else {                                                      \
    return 0;                                                   \
  }                                                             \
}                                                               \
                                                                \
static void name##Free(NaClSrpcArg* arg) {                      \
}                                                               \
                                                                \
static const ArgEltInterface k##name##IoInterface = {           \
  name##Get, name##Put, name##Print, name##Length, name##Free   \
};

/*
 * The three basic parameter types.
 */
BASIC_TYPE_IO_DECLARE(Bool, char, bval, 1d)
BASIC_TYPE_IO_DECLARE(Double, double, dval, f)
BASIC_TYPE_IO_DECLARE(Int, int, ival, d)

#define ARRAY_TYPE_IO_DECLARE(name, impl_type, field, array)                   \
static int name##ArrGet(NaClSrpcChannel* channel,                              \
                     int allocate_memory,                                      \
                     int read_value,                                           \
                     NaClSrpcArg* arg) {                                       \
  uint32_t dimdim;                                                             \
  size_t dim;                                                                  \
                                                                               \
  if (1 != __NaClSrpcImcRead(&dimdim, sizeof(dim), 1, channel)) {              \
    return 0;                                                                  \
  }                                                                            \
  dim = (size_t) dimdim;                                                       \
  if (allocate_memory) {                                                       \
    if (dim > SIZE_T_MAX / sizeof(impl_type)) {                                \
      return 0;                                                                \
    }                                                                          \
    arg->u.field.array = (impl_type*) malloc(dim * sizeof(impl_type));         \
    if (NULL == arg->u.field.array) {                                          \
      return 0;                                                                \
    }                                                                          \
    arg->u.field.count = dim;                                                  \
  } else if (arg->u.field.count < dim) {                                       \
    return 0;                                                                  \
  }                                                                            \
  if (read_value &&                                                            \
      dim != __NaClSrpcImcRead(arg->u.field.array,                             \
                               sizeof(impl_type),                              \
                               dim,                                            \
                               channel)) {                                     \
    return 0;                                                                  \
  }                                                                            \
  return 1;                                                                    \
}                                                                              \
                                                                               \
static int name##ArrPut(NaClSrpcArg* arg,                                      \
                     int write_value,                                          \
                     NaClSrpcChannel* channel) {                               \
  if (1 !=                                                                     \
      __NaClSrpcImcWrite(&arg->u.field.count, sizeof(uint32_t), 1, channel)) { \
    return 0;                                                                  \
  }                                                                            \
  if (write_value) {                                                           \
    return arg->u.field.count ==                                               \
        __NaClSrpcImcWrite(arg->u.field.array,                                 \
                           sizeof(impl_type),                                  \
                           arg->u.field.count,                                 \
                           channel);                                           \
  }                                                                            \
  return 1;                                                                    \
}                                                                              \
                                                                               \
static void name##ArrPrint(NaClSrpcArg* arg) {                                 \
  dprintf(("[%"PRIu32"], array = %p",                                          \
           arg->u.field.count,                                                 \
           (void*) arg->u.field.array));                                       \
}                                                                              \
                                                                               \
static uint32_t name##ArrLength(NaClSrpcArg* arg,                              \
                             int write_value,                                  \
                             int* handles) {                                   \
  *handles = 0;                                                                \
  if (write_value) {                                                           \
    return sizeof(uint32_t) + sizeof(impl_type) * arg->u.field.count;          \
  } else {                                                                     \
    return sizeof(uint32_t);                                                   \
  }                                                                            \
}                                                                              \
                                                                               \
static void name##ArrFree(NaClSrpcArg* arg) {                                  \
  dprintf(("Freeing %p\n", (void*) arg->u.field.array));                       \
  free(arg->u.field.array);                                                    \
  arg->u.field.array = NULL;                                                   \
}                                                                              \
                                                                               \
static const ArgEltInterface k##name##ArrIoInterface = {                       \
  name##ArrGet, name##ArrPut, name##ArrPrint, name##ArrLength, name##ArrFree   \
};

/*
 * The three array parameter types.
 */
ARRAY_TYPE_IO_DECLARE(Char, char, caval, carr)
ARRAY_TYPE_IO_DECLARE(Double, double, daval, darr)
ARRAY_TYPE_IO_DECLARE(Int, int, iaval, iarr)

/*
 * Handle (descriptor) type I/O support.
 */
static int HandleGet(NaClSrpcChannel* channel,
                     int allocate_memory,
                     int read_value,
                     NaClSrpcArg* arg) {
  if (read_value) {
    arg->u.hval = __NaClSrpcImcReadDesc(channel);
  }
  return 1;
}

static int HandlePut(NaClSrpcArg* arg,
                     int write_value,
                     NaClSrpcChannel* channel) {
  if (write_value) {
    return 1 == __NaClSrpcImcWriteDesc(channel, arg->u.hval);
  }
  return 1;
}

static void HandlePrint(NaClSrpcArg* arg) {
#ifdef __native_client__
  dprintf(("%d", arg->u.hval));
#else
  dprintf(("handle"));
#endif  /* __native_client__ */
}

static uint32_t HandleLength(NaClSrpcArg* arg,
                             int write_value,
                             int* handles) {
  if (write_value) {
    *handles = 1;
  } else {
    *handles = 0;
  }
  return 0;
}

static void HandleFree(NaClSrpcArg* arg) {
}

static const ArgEltInterface kHandleIoInterface = {
  HandleGet, HandlePut, HandlePrint, HandleLength, HandleFree
};

/*
 * String type I/O support.
 */
static int StringGet(NaClSrpcChannel* channel,
                     int allocate_memory,
                     int read_value,
                     NaClSrpcArg* arg) {
  uint32_t dimdim;
  size_t dim;

  if (read_value) {
    if (1 != __NaClSrpcImcRead(&dimdim, sizeof(dim), 1, channel)) {
      return 0;
    }
    /*
     * check if dim + 1 (in the malloc below) will result in an
     * integer overflow
     */
    dim = (size_t) dimdim;
    if (dim > SIZE_T_MAX - 1) {
      return 0;
    }
    arg->u.sval = (char*) malloc(dim + 1);
    if (NULL == arg->u.sval) {
      return 0;
    }
    if (dim != __NaClSrpcImcRead(arg->u.sval, sizeof(char), dim, channel)) {
      return 0;
    }
    arg->u.sval[dim] = '\0';
  }
  return 1;
}

static int StringPut(NaClSrpcArg* arg,
                     int write_value,
                     NaClSrpcChannel* channel) {
  if (write_value) {
    uint32_t slen = (uint32_t) strlen(arg->u.sval);
    if (1 != __NaClSrpcImcWrite(&slen, sizeof(slen), 1, channel) ||
        slen !=
        __NaClSrpcImcWrite(arg->u.sval, 1, (size_t) slen, channel)) {
      return 0;
    }
  }
  return 1;
}

static void StringPrint(NaClSrpcArg* arg) {
  dprintf((", strlen %u, '%s'", (unsigned) strlen(arg->u.sval), arg->u.sval));
}

static uint32_t StringLength(NaClSrpcArg* arg,
                             int write_value,
                             int* handles) {
  *handles = 0;
  if (write_value) {
    return sizeof(uint32_t) + strlen(arg->u.sval);
  } else {
    return 0;
  }
}

static void StringFree(NaClSrpcArg* arg) {
  free(arg->u.sval);
  arg->u.sval = NULL;
}

static const ArgEltInterface kStringIoInterface = {
  StringGet, StringPut, StringPrint, StringLength, StringFree
};

/*
 * Invalid type I/O support.
 */
static int InvalidGet(NaClSrpcChannel* channel,
                      int allocate_memory,
                      int read_value,
                      NaClSrpcArg* arg) {
  return 0;
}

static int InvalidPut(NaClSrpcArg* arg,
                      int write_value,
                      NaClSrpcChannel* channel) {
  return 0;
}

static void InvalidPrint(NaClSrpcArg* arg) {
  dprintf(("INVALID"));
}

static uint32_t InvalidLength(NaClSrpcArg* arg,
                              int write_value,
                              int* handles) {
  *handles = 0;
  return 0;
}

static void InvalidFree(NaClSrpcArg* arg) {
}

static const ArgEltInterface kInvalidIoInterface = {
  InvalidGet, InvalidPut, InvalidPrint, InvalidLength, InvalidFree
};

static const ArgEltInterface* ArgsGetEltInterface(NaClSrpcArg* arg) {
  switch (arg->tag) {
   case NACL_SRPC_ARG_TYPE_INVALID:
    return &kInvalidIoInterface;
   case NACL_SRPC_ARG_TYPE_BOOL:
    return &kBoolIoInterface;
   case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    return &kCharArrIoInterface;
   case NACL_SRPC_ARG_TYPE_DOUBLE:
    return &kDoubleIoInterface;
   case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    return &kDoubleArrIoInterface;
   case NACL_SRPC_ARG_TYPE_HANDLE:
    return &kHandleIoInterface;
   case NACL_SRPC_ARG_TYPE_INT:
    return &kIntIoInterface;
   case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    return &kIntArrIoInterface;
   case NACL_SRPC_ARG_TYPE_STRING:
    return &kStringIoInterface;
   case NACL_SRPC_ARG_TYPE_OBJECT:
    return &kInvalidIoInterface;
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    return &kInvalidIoInterface;
   default:
    break;
  }
  return &kInvalidIoInterface;
}

/*
 * Argument vector (Args) I/O interface.
 */
static int ArgsGet(const ArgsIoInterface* argsdesc,
                   NaClSrpcChannel* channel,
                   int allocate_args,
                   int read_values,
                   NaClSrpcArg* argvec[],
                   const char* arg_types) {
  uint32_t lenu32;
  uint32_t i;
  NaClSrpcArg *args = NULL;

  if (1 != __NaClSrpcImcRead(&lenu32, sizeof(lenu32), 1, channel)) {
    return 0;
  }
  if (lenu32 >= NACL_SRPC_MAX_ARGS) {
    return 0;
  }

  if (allocate_args && lenu32 > 0) {
    size_t ix;
    size_t length = (size_t) lenu32;
    if (length > SIZE_T_MAX / sizeof(NaClSrpcArg)) {
      goto error;
    }
    /*
     * post condition: no integer overflow, so
     * length * sizeof(NaClSrpcArg) <= SIZE_T_MAX
     */
    args = (NaClSrpcArg*) malloc(length * sizeof(NaClSrpcArg));
    if (args == NULL) {
      goto error;
    }
    memset((void*) args, 0, length * sizeof(NaClSrpcArg));

    /*
     * Initialize the arg type tags with those specified in the declaration.
     */
    for (ix = 0; ix < length; ++ix) {
      if (arg_types[ix] == ':' || arg_types[ix] == '\0') {
        return 0;
      }
      args[ix].tag = arg_types[ix];
    }
    /* TODO(sehr): include test code to validate arglist mismatches */
    if (arg_types[length] != ':' && arg_types[length] != '\0') {
      return 0;
    }
  } else {
    args = argvec[0];
  }

  for (i = 0; i < lenu32; ++i) {
    char read_type;
    const ArgEltInterface* desc;

    if (1 != __NaClSrpcImcRead(&read_type, sizeof(char), 1, channel)) {
      goto error;
    }
    if (args[i].tag != read_type) {
      goto error;
    }
    /* Set the index pointer to point to the element to be read into */
    argvec[i] = args + i;
    argvec[i]->tag = read_type;
    /* Get the I/O descriptor for the type to be read */
    desc = argsdesc->element_interface(argvec[i]);
    /* Read the element */
    if (!desc->get(channel, allocate_args, read_values, argvec[i])) {
      goto error;
    }
  }
  argvec[lenu32] = NULL;
  return 1;
error:
  if (args != NULL) {
    argsdesc->free(argsdesc, argvec);
  }
  return 0;
}

static int ArgsPut(const ArgsIoInterface* argsdesc,
                   NaClSrpcChannel* channel,
                   int write_value,
                   NaClSrpcArg* argvec[]) {
  uint32_t i;
  uint32_t length;

  for (length = 0; argvec[length] != NULL; ++length) {}
  if (length >= NACL_SRPC_MAX_ARGS) {
    return 0;
  }
  if (1 != __NaClSrpcImcWrite(&length, sizeof(length), 1, channel)) {
    return 0;
  }

  for (i = 0; i < length; ++i) {
    const ArgEltInterface* desc = argsdesc->element_interface(argvec[i]);
    /* The tag */
    if (1 != __NaClSrpcImcWrite(&argvec[i]->tag, sizeof(char), 1, channel)) {
      return 0;
    }
    /* And the individual type */
    if (!desc->put(argvec[i], write_value, channel)) {
      return 0;
    }
  }
  return 1;
}

static int ArgsLength(const ArgsIoInterface* argsdesc,
                      NaClSrpcArg* argvec[],
                      int write_value,
                      uint32_t* bytes,
                      uint32_t* handles) {
  int i;
  size_t tmp_bytes;
  int tmp_handles;

  /* Initialize the reported results */
  *bytes = 0;
  *handles = 0;
  /* Initialize to pass the vector length, and no handles. */
  tmp_bytes = sizeof(uint32_t);
  tmp_handles = 0;
  for (i = 0; i < NACL_SRPC_MAX_ARGS && NULL != argvec[i]; ++i) {
    int handle_cnt;
    const ArgEltInterface* desc = argsdesc->element_interface(argvec[i]);
    /* The arg tag + the argument */
    tmp_bytes += 1 + desc->length(argvec[i], write_value, &handle_cnt);
    tmp_handles += handle_cnt;
  }
  if (NACL_SRPC_MAX_ARGS == i) {
    return 0;
  }
  *bytes = tmp_bytes;
  *handles = tmp_handles;
  return 1;
}

static void ArgsFree(const ArgsIoInterface* argsdesc,
                     NaClSrpcArg* argvec[]) {
  NaClSrpcArg** argvecp;

  for (argvecp = argvec; *argvecp != NULL; ++argvecp) {
    const ArgEltInterface* desc = argsdesc->element_interface(*argvecp);
    desc->free(*argvecp);
  }
  free(argvec[0]);
}

static const struct ArgsIoInterface kArgsIoInterface = {
  ArgsGet, ArgsPut, ArgsLength, ArgsFree, ArgsGetEltInterface
};

static const ArgsIoInterface* GetArgsInterface(uint32_t protocol_version) {
  /* Future versioning of the argument vector I/O goes here. */
  return &kArgsIoInterface;
}

/*
 * RpcRead attempts to read a message header from the specified channel.
 * It returns 1 if successful, and 0 otherwise.
 */
static int RpcRead(NaClSrpcChannel* channel,
                   NaClSrpcRpc* rpc) {
  uint32_t protocol;
  uint8_t is_req = 0;
  uint64_t request_id = 0;
  uint32_t rpc_num = 0;
  NaClSrpcError app_err = NACL_SRPC_RESULT_OK;

  dprintf((SIDE "RpcRead starting\n"));
  if (1 != __NaClSrpcImcRead(&protocol, sizeof(protocol), 1, channel)) {
    dprintf((SIDE "READ: protocol read fail\n"));
    return 0;
  }
  rpc->protocol_version = protocol;
  /*
   * If there are protocol version specific rpc fields, a version dispatcher
   * would need to go here.
   */
  if (1 != __NaClSrpcImcRead(&request_id, sizeof(request_id), 1, channel)) {
    dprintf((SIDE "RpcRead: request_id read fail\n"));
    return 0;
  }
  rpc->request_id = request_id;
  if (1 != __NaClSrpcImcRead(&is_req, sizeof(is_req), 1, channel)) {
    dprintf((SIDE "RpcRead: is_request read fail\n"));
    return 0;
  }
  rpc->is_request = is_req;
  if (1 != __NaClSrpcImcRead(&rpc_num, sizeof(rpc_num), 1, channel)) {
    dprintf((SIDE "RpcRead: rpc_number read fail\n"));
    return 0;
  }
  rpc->rpc_number = rpc_num;
  if (!rpc->is_request) {
    /* Responses also need to read the app_error member */
    if (1 != __NaClSrpcImcRead(&app_err, sizeof(app_err), 1, channel)) {
      dprintf((SIDE "RpcRead: app_error read fail\n"));
      return 0;
    }
  }
  rpc->app_error = app_err;
  dprintf((SIDE "RpcRead(%"PRIx32", %s, %"PRIu32", %s) done\n",
           rpc->protocol_version,
           (rpc->is_request == 0) ? "response" : "request",
           rpc->rpc_number,
           NaClSrpcErrorString(rpc->app_error)));
  return 1;
}

/*
 * RpcWrite writes an RPC header on the specified channel.  We can only
 * send the current protocol version.
 */
static int RpcWrite(const ArgsIoInterface* desc,
                    NaClSrpcChannel* channel,
                    NaClSrpcRpc* rpc) {
  dprintf((SIDE "RpcWrite(%"PRIx32", %s, %"PRIu32", %s)\n",
           rpc->protocol_version,
           rpc->is_request ? "request" : "response",
           rpc->rpc_number,
           NaClSrpcErrorString(rpc->app_error)));
  if (1 !=
      __NaClSrpcImcWrite(&rpc->protocol_version,
                         sizeof(rpc->protocol_version),
                         1,
                         channel)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->request_id,
                         sizeof(rpc->request_id),
                         1,
                         channel)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->is_request,
                         sizeof(rpc->is_request),
                         1,
                         channel)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->rpc_number,
                         sizeof(rpc->rpc_number),
                         1,
                         channel)) {
    return 0;
  }
  /* Responses also need to send the app_error member */
  if (!rpc->is_request) {
    if (1 !=
        __NaClSrpcImcWrite(&rpc->app_error,
                           sizeof(rpc->app_error),
                           1,
                           channel)) {
      return 0;
    }
  }
  dprintf((SIDE "RpcWrite done\n"));
  return 1;
}

static void RpcLength(const ArgsIoInterface* desc,
                      int is_request,
                      uint32_t* bytes,
                      uint32_t* handles) {
  *bytes =
      sizeof(uint32_t) +  /* protocol_version */
      sizeof(uint64_t) +  /* request_id */
      sizeof(uint8_t)  +  /* is_request */
      sizeof(uint32_t) +  /* rpc_number */
      (is_request ? sizeof(uint32_t) : 0);  /* app_error */
  *handles = 0;
}

/*
 * Receive a message from the specified channel.  If the next message on
 * the channel is a response, it is simply returned.  If the next message
 * is a request, it is returned, and a template of a response is returned
 * via the pointer passed in.
 */
int NaClSrpcRequestRead(NaClSrpcChannel* channel,
                        NaClSrpcRpc* rpc,
                        NaClSrpcArg* args[],
                        NaClSrpcArg* rets[]) {
  uint32_t retval;
  const ArgsIoInterface* desc;
  const char* rpc_name;
  const char* in_types;
  const char* out_types;

  /* Read the common rpc structure (0 indicates failure) */
  if (!RpcRead(channel, rpc)) {
    dprintf((SIDE "RequestRead: rpc read failed\n"));
    /* Clear the rest of the buffer contents to ensure alignment */
    __NaClSrpcImcMarkReadBufferEmpty(channel);
    return 0;
  }
  /* Get the Args I/O descriptor for the protocol version read */
  desc = GetArgsInterface(rpc->protocol_version);
  /* Process request or response */
  if (!rpc->is_request) {
    return 0;
  }
  /* Announce start of request processing */
  dprintf((SIDE "RequestRead: rpc %"PRIu32"\n", rpc->rpc_number));
  /* Get types for receiving args and rets */
  retval = NaClSrpcGetArgTypes(channel,
                               rpc->rpc_number,
                               &rpc_name,
                               &in_types,
                               &out_types);
  if (!retval) {
    dprintf(("RequestRead: bad rpc number in request\n"));
    return 0;
  }
  if (!desc->get(desc, channel, 1, 1, args, in_types)) {
    dprintf(("RequestRead: argument vector receive failed\n"));
    return 0; /* get frees memory on error. */
  }
  /* Construct the rets from the channel. */
  if (!desc->get(desc, channel, 1, 0, rets, out_types)) {
    dprintf(("RequestRead: rets template receive failed\n"));
    desc->free(desc, args);
    return 0;
  }
  dprintf((SIDE "RequestRead(%p, %"PRIu32", %s) received\n",
           (void*) channel, rpc->rpc_number, rpc_name));
  return 1;
}

static int RequestWrite(const ArgsIoInterface* desc,
                        NaClSrpcRpc* rpc,
                        NaClSrpcArg* args[],
                        NaClSrpcArg* rets[],
                        NaClSrpcChannel* channel) {
  dprintf(("RequestWrite(%p, %"PRIu32")\n", (void*) channel, rpc->rpc_number));
  /* Set up and send rpc */
  rpc->is_request = 1;
  rpc->app_error = NACL_SRPC_RESULT_OK;
  if (!RpcWrite(desc, channel, rpc)) {
    return 0;
  }
  /* Then send the args */
  if (!desc->put(desc, channel, 1, args)) {
    dprintf(("RequestWrite: args send failed\n"));
    return 0;
  }
  /* And finally the rets template */
  if (!desc->put(desc, channel, 0, rets)) {
    dprintf(("RequestWrite: rets template send failed\n"));
    return 0;
  }
  if (!__NaClSrpcImcFlush(channel)) {
    /* Requests with bad handles could fail.  Report to the caller. */
    dprintf(("RequestWrite(%p, %"PRIu32") failed\n",
             (void*) channel, rpc->rpc_number));
    return 0;
  }
  dprintf(("RequestWrite(%p, %"PRIu32") sent\n",
           (void*) channel, rpc->rpc_number));
  return 1;
}

static int RequestLength(const ArgsIoInterface* desc,
                         NaClSrpcArg* args[],
                         NaClSrpcArg* rets[],
                         uint32_t* bytes,
                         uint32_t* handles) {
  uint32_t tmp_bytes;
  uint32_t tmp_hdl;

  RpcLength(desc, 1, &tmp_bytes, &tmp_hdl);
  *bytes += tmp_bytes;
  *handles += tmp_hdl;
  if (!desc->length(desc, args, 1, &tmp_bytes, &tmp_hdl)) {
    return 0;
  }
  *bytes += tmp_bytes;
  *handles += tmp_hdl;
  if (!desc->length(desc, rets, 0, &tmp_bytes, &tmp_hdl)) {
    return 0;
  }
  *bytes += tmp_bytes;
  *handles += tmp_hdl;

  return 1;
}

NaClSrpcError NaClSrpcRequestWrite(NaClSrpcChannel* channel,
                                   NaClSrpcRpc* rpc,
                                   NaClSrpcArg* args[],
                                   NaClSrpcArg* rets[]) {
  uint32_t bytes;
  uint32_t handles;
  const ArgsIoInterface* desc = GetArgsInterface(rpc->protocol_version);

  if (!RequestLength(desc, args, rets, &bytes, &handles)) {
    return NACL_SRPC_RESULT_INTERNAL;
  }
  if (!RequestWrite(desc, rpc, args, rets, channel)) {
    return NACL_SRPC_RESULT_INTERNAL;
  }
  return NACL_SRPC_RESULT_OK;
}

/*
 * Receive a message from the specified channel.  If the next message on
 * the channel is a response, it is simply returned.  If the next message
 * is a request, it is returned, and a template of a response is returned
 * via the pointer passed in.
 */
int NaClSrpcResponseRead(NaClSrpcChannel* channel,
                         NaClSrpcRpc* rpc,
                         NaClSrpcArg* rets[]) {
  uint32_t retval;
  const ArgsIoInterface* desc;
  const char* rpc_name;
  const char* in_types;
  const char* out_types;


  /* Read the common rpc structure (0 indicates failure) */
  if (!RpcRead(channel, rpc)) {
    dprintf((SIDE "ResponseRead: rpc read failed\n"));
    /* Clear the rest of the buffer contents to ensure alignment */
    __NaClSrpcImcMarkReadBufferEmpty(channel);
    return 0;
  }
  /* Get the Args I/O descriptor for the protocol version read */
  desc = GetArgsInterface(rpc->protocol_version);
  /* If the message is a request, return an error */
  if (rpc->is_request) {
    return 0;
  }
  /* Announce start of response processing */
  dprintf((SIDE "ResponseRead: response, rpc %"PRIu32"\n", rpc->rpc_number));
  /* Get types for receiving rets */
  retval = NaClSrpcGetArgTypes(channel,
                               rpc->rpc_number,
                               &rpc_name,
                               &in_types,
                               &out_types);
  if (!retval) {
    dprintf((SIDE "ResponseRead: bad rpc number\n"));
    return 0;
  }
  if (NACL_SRPC_RESULT_OK != rpc->app_error) {
    dprintf(("ResponseRead: method returned failure: %d\n", rpc->app_error));
    /* Clear the rest of the buffer contents to ensure alignment */
    __NaClSrpcImcMarkReadBufferEmpty(channel);
    return 1;
  }
  dprintf((SIDE "ResponseRead: getting rets\n"));
  if (!desc->get(desc, channel, 0, 1, rets, out_types)) {
    dprintf(("ResponseRead: rets receive failed\n"));
    /* Clear the rest of the buffer contents to ensure alignment */
    __NaClSrpcImcMarkReadBufferEmpty(channel);
    /* get cleans up argument memory before returning */
    return 0;
  }
  dprintf((SIDE "ResponseRead(%p, %"PRIu32", %s) received\n",
           (void*) channel, rpc->rpc_number, rpc_name));
  return 1;
}

static int ResponseWrite(const ArgsIoInterface* desc,
                         NaClSrpcRpc* rpc,
                         NaClSrpcArg* rets[],
                         NaClSrpcChannel* channel) {
  dprintf((SIDE "SRPC: ResponseWrite\n"));
  rpc->is_request = 0;
  if (!RpcWrite(desc, channel, rpc)) {
    return 0;
  }
  if (!desc->put(desc, channel, 1, rets)) {
    dprintf(("SRPC: rets send failed\n"));
    return 0;
  }
  if (!__NaClSrpcImcFlush(channel)) {
    /*
     * If the put call fails due to a bad handle, the transport layer
     * doesn't send anything.  Therefore we need to return an error message
     * so that the client doesn't wait forever.
     */
    rpc->app_error = NACL_SRPC_RESULT_INTERNAL;
    rpc->is_request = 0;
    if (!RpcWrite(desc, channel, rpc)) {
      return 0;
    }
    __NaClSrpcImcFlush(channel);
    dprintf(("SRPC: transmission error occurred\n"));
  }
  dprintf((SIDE "ResponseWrite(%p, %"PRIu32", %d, %s): sent\n",
           (void*) channel, rpc->rpc_number, rpc->app_error,
           NaClSrpcErrorString(rpc->app_error)));
  return 1;
}

static int ResponseLength(const ArgsIoInterface* desc,
                          NaClSrpcArg* rets[],
                          uint32_t* bytes,
                          uint32_t* handles) {
  uint32_t tmp_bytes;
  uint32_t tmp_hdl;

  RpcLength(desc, 0, &tmp_bytes, &tmp_hdl);
  *bytes += tmp_bytes;
  *handles += tmp_hdl;
  if (!desc->length(desc, rets, 1, &tmp_bytes, &tmp_hdl)) {
    return 0;
  }
  *bytes += tmp_bytes;
  *handles += tmp_hdl;

  return 1;
}

NaClSrpcError NaClSrpcResponseWrite(NaClSrpcChannel* channel,
                                    NaClSrpcRpc* rpc,
                                    NaClSrpcArg* rets[]) {
  uint32_t bytes;
  uint32_t handles;
  const ArgsIoInterface* desc = GetArgsInterface(rpc->protocol_version);

  /*
   * ResponseLength computes the requirements for a write buffer.
   * It is currently unused, but will be used in the next CL to separate
   * serialization from buffer send/receive.
   */
  if (!ResponseLength(desc, rets, &bytes, &handles)) {
    return NACL_SRPC_RESULT_INTERNAL;
  }
  if (!ResponseWrite(desc, rpc, rets, channel)) {
    return NACL_SRPC_RESULT_INTERNAL;
  }
  return NACL_SRPC_RESULT_OK;
}
