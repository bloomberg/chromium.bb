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
 * The receive/dispatch function returns an enum indicating how the enclosing
 * loop should proceed.
 */
typedef enum {
  DISPATCH_CONTINUE,  /* Continue receive-dispatch loop */
  DISPATCH_BREAK,     /* Break out of loop was requested by invoked method */
  DISPATCH_RESPONSE,  /* Instead of a request, we received a response */
  DISPATCH_EOF,       /* No more requests or responses can be received */
} DispatchReturn;

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
  int (*get)(NaClSrpcImcBuffer* buffer,
             int allocate,
             int read_value,
             NaClSrpcArg* arg);
  int (*put)(const NaClSrpcArg* arg,
             int write_value,
             NaClSrpcImcBuffer* buffer);
  void (*print)(const NaClSrpcArg* arg);
  uint32_t (*length)(const NaClSrpcArg* arg, int write_value, int* descs);
  void (*free)(NaClSrpcArg* arg);
};

/*
 * Argument lists are serialized using a protocol version specific interface.
 */
typedef struct ArgsIoInterface ArgsIoInterface;
struct ArgsIoInterface {
  int (*get)(const ArgsIoInterface* argsdesc,
             NaClSrpcImcBuffer* buffer,
             int allocate_args,
             int read_values,
             NaClSrpcArg* argvec[],
             const char* arg_types);
  int (*put)(const ArgsIoInterface* argsdesc,
             NaClSrpcImcBuffer* buffer,
             int write_value,
             NaClSrpcArg* argvec[]);
  int (*length)(const ArgsIoInterface* argsdesc,
                NaClSrpcArg* argvec[],
                int write_value,
                uint32_t* bytes,
                uint32_t* handles);
  void (*free)(const ArgsIoInterface* argsdesc,
               NaClSrpcArg* argvec[]);
  const ArgEltInterface* (*element_interface)(const NaClSrpcArg* arg);
};
static const ArgsIoInterface* GetArgsInterface(uint32_t protocol_version);


static DispatchReturn NaClSrpcReceiveAndDispatch(NaClSrpcChannel* channel,
                                                 NaClSrpcRpc* rpc_stack_top) {
  NaClSrpcImcBuffer* buffer;
  NaClSrpcRpc rpc;
  const char* rpc_name;
  const char* arg_types;
  const char* ret_types;
  NaClSrpcArg* args[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg* rets[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcMethod method;
  int retval;
  int return_break = 0;
  double this_start_usec = 0.0;
  double this_method_usec;
  const ArgsIoInterface* desc;

  dprintf((SIDE "ReceiveAndDispatch: %p\n", (void*) rpc_stack_top));
  /* If we are timing, get the start time. */
  if (channel->timing_enabled) {
    this_start_usec = __NaClSrpcGetUsec();
  }
  /* Read a message from the channel. */
  buffer = __NaClSrpcImcFillbuf(channel);
  if (NULL == buffer) {
    dprintf((SIDE "ReceiveAndDispatch: buffer read failed\n"));
    return DISPATCH_EOF;
  }
  /* Deserialize the header (0 indicates failure) */
  if (!NaClSrpcRpcGet(buffer, &rpc)) {
    dprintf((SIDE "ReceiveAndDispatch: rpc deserialize failed\n"));
    /* Drop the current request and continue */
    return DISPATCH_CONTINUE;
  }
  if (rpc.is_request) {
    /* This is a new request. */
    if (NULL == channel->server) {
      if (NULL == rpc_stack_top) {
        /* There is no service to dispatch requests. Abort. */
        return DISPATCH_EOF;
      } else {
        /* Inform the pending invoke that a failure happened. */
        rpc_stack_top->app_error = NACL_SRPC_RESULT_INTERNAL;
        return DISPATCH_BREAK;
      }
    }
    /* Fall through to request handling below. */
  } else {
    /* This is a response to a pending request. */
    if (NULL == rpc_stack_top) {
      /* There is no pending request. Abort. */
      return DISPATCH_BREAK;
    } else {
      if (rpc.request_id == rpc_stack_top->request_id) {
        /* Back up to the start of the message and process it as a response. */
        rpc_stack_top->buffer = buffer;
        return DISPATCH_RESPONSE;
      } else {
        /* Received an out-of-order response.  Abort. */
        return DISPATCH_BREAK;
      }
    }
  }
  /* Get types for receiving args and rets */
  retval = NaClSrpcServiceMethodNameAndTypes(channel->server,
                                             rpc.rpc_number,
                                             &rpc_name,
                                             &arg_types,
                                             &ret_types);
  if (!retval) {
    dprintf(("RequestGet: bad rpc number in request\n"));
    /* Drop the request with a bad rpc number and continue */
    return DISPATCH_CONTINUE;
  }
  /* Deserialize the request from the buffer. */
  if (!NaClSrpcRequestGet(buffer, &rpc, arg_types, args, ret_types, rets)) {
    dprintf((SIDE "ReceiveAndDispatch: receive message failed\n"));
    return DISPATCH_EOF;
  }
  desc = GetArgsInterface(rpc.protocol_version);
  /* Then we invoke the method, which computes a return code. */
  method = NaClSrpcServiceMethod(channel->server, rpc.rpc_number);
  if (NULL == method) {
    dprintf((SIDE "ReceiveAndDispatch: bad rpc number %"PRIu32"\n",
             rpc.rpc_number));
    return DISPATCH_CONTINUE;
  }
  rpc.app_error = (*method)(channel, args, rets);
  if (NACL_SRPC_RESULT_BREAK == rpc.app_error) {
    return_break = 1;
    rpc.app_error = NACL_SRPC_RESULT_OK;
  }
  /* Then we return the rets. */
  retval = NaClSrpcResponseWrite(channel, &rpc, rets);
  /* Then we free the memory for the args and rets. */
  desc->free(desc, args);
  desc->free(desc, rets);
  if (!retval) {
    /* If the response write failed, drop request and continue. */
    dprintf((SIDE "ReceiveAndDispatch: response write failed\n"));
    return DISPATCH_CONTINUE;
  }
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
    return DISPATCH_BREAK;
  } else {
    return DISPATCH_CONTINUE;
  }
}

/*
 * After an RPC is sent, the current thread can block waiting for the
 * response.  As currently there is only one thread receiving messages,
 * this thread will handle receiving and dispatching while waiting for
 * its response.  This allows for one thread to process calls back and
 * forth between client and server, as required for the NPAPI main thread
 * in Pepper.
 */
void NaClSrpcRpcWait(NaClSrpcChannel* channel,
                     NaClSrpcRpc* rpc) {
  DispatchReturn retval;

  /*
   * Loop receiving RPCs and processing them.
   * The loop stops when the receive/dispatch function returns.
   */
  do {
    retval = NaClSrpcReceiveAndDispatch(channel, rpc);
  } while (DISPATCH_CONTINUE == retval);
  /* Process responses */
  dprintf((SIDE "response to RpcWait: %p, %d\n", (void*) rpc, retval));
  if (NULL != rpc &&
      DISPATCH_RESPONSE == retval) {
    NaClSrpcImcBuffer* buffer = rpc->buffer;
    /* We know here that the buffer contains a response to the current rpc. */
    __NaClSrpcImcRefill(buffer);
    /* Deserialize the header (0 indicates failure) */
    if (!NaClSrpcRpcGet(buffer, rpc)) {
      dprintf((SIDE "InvokeV: rpc deserialize failed\n"));
      rpc->app_error = NACL_SRPC_RESULT_INTERNAL;
      return;
    }
    /* Paranoia: if the message is a request, return an error */
    if (rpc->is_request) {
      dprintf(("Response: rpc is not response: %d\n", rpc->is_request));
      rpc->app_error = NACL_SRPC_RESULT_INTERNAL;
      return;
    }
    if (!NaClSrpcResponseGet(buffer, rpc, rpc->ret_types, rpc->rets)) {
      dprintf(("SRPC: response receive failed\n"));
      rpc->app_error = NACL_SRPC_RESULT_INTERNAL;
      return;
    }
  }
}

/*
 * Argument element I/O interfaces.
 */

#define BASIC_TYPE_IO_DECLARE(name, impl_type, field, format)   \
static int name##Get(NaClSrpcImcBuffer* buffer,                 \
                     int allocate_memory,                       \
                     int read_value,                            \
                     NaClSrpcArg* arg) {                        \
  UNREFERENCED_PARAMETER(allocate_memory);                      \
  if (read_value &&                                             \
      1 != __NaClSrpcImcRead(buffer,                            \
                             sizeof(impl_type),                 \
                             1,                                 \
                             &arg->u.field)) {                  \
    return 0;                                                   \
  }                                                             \
  return 1;                                                     \
}                                                               \
                                                                \
static int name##Put(const NaClSrpcArg* arg,                    \
                     int write_value,                           \
                     NaClSrpcImcBuffer* buffer) {               \
  if (write_value) {                                            \
    return 1 == __NaClSrpcImcWrite(&arg->u.field,               \
                                   sizeof(impl_type),           \
                                   1,                           \
                                   buffer);                     \
  }                                                             \
  return 1;                                                     \
}                                                               \
                                                                \
static void name##Print(const NaClSrpcArg* arg) {               \
  dprintf(("%"#format"", arg->u.field));                        \
}                                                               \
                                                                \
static uint32_t name##Length(const NaClSrpcArg* arg,            \
                             int write_value,                   \
                             int* handles) {                    \
  UNREFERENCED_PARAMETER(arg);                                  \
  *handles = 0;                                                 \
  if (write_value) {                                            \
    return sizeof(impl_type);                                   \
  } else {                                                      \
    return 0;                                                   \
  }                                                             \
}                                                               \
                                                                \
static void name##Free(NaClSrpcArg* arg) {                      \
  UNREFERENCED_PARAMETER(arg);                                  \
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
static int name##ArrGet(NaClSrpcImcBuffer* buffer,                             \
                        int allocate_memory,                                   \
                        int read_value,                                        \
                        NaClSrpcArg* arg) {                                    \
  uint32_t dimdim;                                                             \
  size_t dim;                                                                  \
                                                                               \
  if (1 != __NaClSrpcImcRead(buffer, sizeof(dimdim), 1, &dimdim)) {            \
    return 0;                                                                  \
  }                                                                            \
  dim = (size_t) dimdim;                                                       \
  if (allocate_memory) {                                                       \
    if (dim >= SIZE_T_MAX / sizeof(*arg->u.field.array)) {                     \
      return 0;                                                                \
    }                                                                          \
    arg->u.field.array =                                                       \
        (impl_type*) malloc(dim * sizeof(*arg->u.field.array));                \
    if (NULL == arg->u.field.array) {                                          \
      return 0;                                                                \
    }                                                                          \
    arg->u.field.count = dim;                                                  \
  } else if (arg->u.field.count < dim) {                                       \
    return 0;                                                                  \
  }                                                                            \
  if (read_value &&                                                            \
      dim != __NaClSrpcImcRead(buffer,                                         \
                               sizeof(impl_type),                              \
                               dim,                                            \
                               arg->u.field.array)) {                          \
    return 0;                                                                  \
  }                                                                            \
  return 1;                                                                    \
}                                                                              \
                                                                               \
static int name##ArrPut(const NaClSrpcArg* arg,                                \
                        int write_value,                                       \
                        NaClSrpcImcBuffer* buffer) {                           \
  if (1 !=                                                                     \
      __NaClSrpcImcWrite(&arg->u.field.count, sizeof(uint32_t), 1, buffer)) {  \
    return 0;                                                                  \
  }                                                                            \
  if (write_value) {                                                           \
    return arg->u.field.count ==                                               \
        __NaClSrpcImcWrite(arg->u.field.array,                                 \
                           sizeof(impl_type),                                  \
                           arg->u.field.count,                                 \
                           buffer);                                            \
  }                                                                            \
  return 1;                                                                    \
}                                                                              \
                                                                               \
static void name##ArrPrint(const NaClSrpcArg* arg) {                           \
  dprintf(("[%"PRIu32"], array = %p",                                          \
           arg->u.field.count,                                                 \
           (void*) arg->u.field.array));                                       \
}                                                                              \
                                                                               \
static uint32_t name##ArrLength(const NaClSrpcArg* arg,                        \
                                int write_value,                               \
                                int* handles) {                                \
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
static int HandleGet(NaClSrpcImcBuffer* buffer,
                     int allocate_memory,
                     int read_value,
                     NaClSrpcArg* arg) {
  UNREFERENCED_PARAMETER(allocate_memory);
  if (read_value) {
    arg->u.hval = __NaClSrpcImcReadDesc(buffer);
  }
  return 1;
}

static int HandlePut(const NaClSrpcArg* arg,
                     int write_value,
                     NaClSrpcImcBuffer* buffer) {
  if (write_value) {
    return 1 == __NaClSrpcImcWriteDesc(arg->u.hval, buffer);
  }
  return 1;
}

static void HandlePrint(const NaClSrpcArg* arg) {
#ifdef __native_client__
  dprintf(("%d", arg->u.hval));
#else
  UNREFERENCED_PARAMETER(arg);
  dprintf(("handle"));
#endif  /* __native_client__ */
}

static uint32_t HandleLength(const NaClSrpcArg* arg,
                             int write_value,
                             int* handles) {
  UNREFERENCED_PARAMETER(arg);
  if (write_value) {
    *handles = 1;
  } else {
    *handles = 0;
  }
  return 0;
}

static void HandleFree(NaClSrpcArg* arg) {
  UNREFERENCED_PARAMETER(arg);
}

static const ArgEltInterface kHandleIoInterface = {
  HandleGet, HandlePut, HandlePrint, HandleLength, HandleFree
};

/*
 * String type I/O support.
 */
static int StringGet(NaClSrpcImcBuffer* buffer,
                     int allocate_memory,
                     int read_value,
                     NaClSrpcArg* arg) {
  uint32_t dimdim;
  size_t dim;

  UNREFERENCED_PARAMETER(allocate_memory);
  UNREFERENCED_PARAMETER(arg);
  if (read_value) {
    if (1 != __NaClSrpcImcRead(buffer, sizeof(dimdim), 1, &dimdim)) {
      return 0;
    }
    /*
     * check if dim + 1 (in the malloc below) will result in an
     * integer overflow
     */
    dim = (size_t) dimdim;
    if (dim >= SIZE_T_MAX) {
      return 0;
    }
    arg->u.sval = (char*) malloc(dim + 1);
    if (NULL == arg->u.sval) {
      return 0;
    }
    if (dim != __NaClSrpcImcRead(buffer, sizeof(char), dim, arg->u.sval)) {
      return 0;
    }
    arg->u.sval[dim] = '\0';
  }
  return 1;
}

static int StringPut(const NaClSrpcArg* arg,
                     int write_value,
                     NaClSrpcImcBuffer* buffer) {
  if (write_value) {
    uint32_t slen = (uint32_t) strlen(arg->u.sval);
    if (1 != __NaClSrpcImcWrite(&slen, sizeof(slen), 1, buffer) ||
        slen !=
        __NaClSrpcImcWrite(arg->u.sval, 1, (size_t) slen, buffer)) {
      return 0;
    }
  }
  return 1;
}

static void StringPrint(const NaClSrpcArg* arg) {
  dprintf((", strlen %u, '%s'", (unsigned) strlen(arg->u.sval), arg->u.sval));
}

static uint32_t StringLength(const NaClSrpcArg* arg,
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
static int InvalidGet(NaClSrpcImcBuffer* buffer,
                      int allocate_memory,
                      int read_value,
                      NaClSrpcArg* arg) {
  UNREFERENCED_PARAMETER(buffer);
  UNREFERENCED_PARAMETER(allocate_memory);
  UNREFERENCED_PARAMETER(read_value);
  UNREFERENCED_PARAMETER(arg);
  return 0;
}

static int InvalidPut(const NaClSrpcArg* arg,
                      int write_value,
                      NaClSrpcImcBuffer* buffer) {
  UNREFERENCED_PARAMETER(arg);
  UNREFERENCED_PARAMETER(write_value);
  UNREFERENCED_PARAMETER(buffer);
  return 0;
}

static void InvalidPrint(const NaClSrpcArg* arg) {
  UNREFERENCED_PARAMETER(arg);
  dprintf(("INVALID"));
}

static uint32_t InvalidLength(const NaClSrpcArg* arg,
                              int write_value,
                              int* handles) {
  UNREFERENCED_PARAMETER(arg);
  UNREFERENCED_PARAMETER(write_value);
  *handles = 0;
  return 0;
}

static void InvalidFree(NaClSrpcArg* arg) {
  UNREFERENCED_PARAMETER(arg);
}

static const ArgEltInterface kInvalidIoInterface = {
  InvalidGet, InvalidPut, InvalidPrint, InvalidLength, InvalidFree
};

/*
 * Argument vector (Args) I/O interface.
 */
static int ArgsGet(const ArgsIoInterface* argsdesc,
                   NaClSrpcImcBuffer* buffer,
                   int allocate_args,
                   int read_values,
                   NaClSrpcArg* argvec[],
                   const char* arg_types) {
  uint32_t lenu32;
  uint32_t i;
  NaClSrpcArg *args = NULL;

  if (1 != __NaClSrpcImcRead(buffer, sizeof(lenu32), 1, &lenu32)) {
    return 0;
  }
  if (lenu32 >= NACL_SRPC_MAX_ARGS) {
    return 0;
  }

  if (allocate_args && lenu32 > 0) {
    size_t ix;
    size_t length = (size_t) lenu32;
    if (length >= SIZE_T_MAX / sizeof(*args)) {
      goto error;
    }
    /*
     * post condition: no integer overflow, so
     * length * sizeof(*args) <= SIZE_T_MAX
     */
    args = (NaClSrpcArg*) malloc(length * sizeof(*args));
    if (args == NULL) {
      goto error;
    }
    memset((void*) args, 0, length * sizeof(*args));

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

    if (1 != __NaClSrpcImcRead(buffer, sizeof(read_type), 1, &read_type)) {
      goto error;
    }
    if (args[i].tag != (enum NaClSrpcArgType) read_type) {
      goto error;
    }
    /* Set the index pointer to point to the element to be read into */
    argvec[i] = args + i;
    argvec[i]->tag = read_type;
    /* Get the I/O descriptor for the type to be read */
    desc = argsdesc->element_interface(argvec[i]);
    /* Read the element */
    if (!desc->get(buffer, allocate_args, read_values, argvec[i])) {
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
                   NaClSrpcImcBuffer* buffer,
                   int write_value,
                   NaClSrpcArg* argvec[]) {
  uint32_t i;
  uint32_t length;

  for (length = 0; argvec[length] != NULL; ++length) {}
  if (length >= NACL_SRPC_MAX_ARGS) {
    return 0;
  }
  if (1 != __NaClSrpcImcWrite(&length, sizeof(length), 1, buffer)) {
    return 0;
  }

  for (i = 0; i < length; ++i) {
    const ArgEltInterface* desc = argsdesc->element_interface(argvec[i]);
    /* The tag */
    if (1 != __NaClSrpcImcWrite(&argvec[i]->tag, sizeof(char), 1, buffer)) {
      return 0;
    }
    /* And the individual type */
    if (!desc->put(argvec[i], write_value, buffer)) {
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

static const ArgEltInterface* ArgsGetEltInterface(const NaClSrpcArg* arg) {
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

static const struct ArgsIoInterface kArgsIoInterface = {
  ArgsGet, ArgsPut, ArgsLength, ArgsFree, ArgsGetEltInterface
};

static const ArgsIoInterface* GetArgsInterface(uint32_t protocol_version) {
  /* Future versioning of the argument vector I/O goes here. */
  UNREFERENCED_PARAMETER(protocol_version);
  return &kArgsIoInterface;
}

/*
 * NaClSrpcRpcGet reads a message header from the specified buffer.
 * It returns 1 if successful, and 0 otherwise.
 */
int NaClSrpcRpcGet(NaClSrpcImcBuffer* buffer,
                   NaClSrpcRpc* rpc) {
  uint32_t protocol;
  uint8_t is_req = 0;
  uint64_t request_id = 0;
  uint32_t rpc_num = 0;
  NaClSrpcError app_err = NACL_SRPC_RESULT_OK;

  dprintf((SIDE "RpcGet starting\n"));
  if (1 != __NaClSrpcImcRead(buffer, sizeof(protocol), 1, &protocol)) {
    dprintf((SIDE "READ: protocol read fail\n"));
    return 0;
  }
  rpc->protocol_version = protocol;
  /*
   * If there are protocol version specific rpc fields, a version dispatcher
   * would need to go here.
   */
  if (1 != __NaClSrpcImcRead(buffer, sizeof(request_id), 1, &request_id)) {
    dprintf((SIDE "RpcGet: request_id read fail\n"));
    return 0;
  }
  rpc->request_id = request_id;
  if (1 != __NaClSrpcImcRead(buffer, sizeof(is_req), 1, &is_req)) {
    dprintf((SIDE "RpcGet: is_request read fail\n"));
    return 0;
  }
  rpc->is_request = is_req;
  if (1 != __NaClSrpcImcRead(buffer, sizeof(rpc_num), 1, &rpc_num)) {
    dprintf((SIDE "RpcGet: rpc_number read fail\n"));
    return 0;
  }
  rpc->rpc_number = rpc_num;
  if (!rpc->is_request) {
    /* Responses also need to read the app_error member */
    if (1 != __NaClSrpcImcRead(buffer, sizeof(app_err), 1, &app_err)) {
      dprintf((SIDE "RpcGet: app_error read fail\n"));
      return 0;
    }
  }
  rpc->app_error = app_err;
  dprintf((SIDE "RpcGet(%"PRIx32", %s, %"PRIu32", %s) done\n",
           rpc->protocol_version,
           (rpc->is_request == 0) ? "response" : "request",
           rpc->rpc_number,
           NaClSrpcErrorString(rpc->app_error)));
  return 1;
}

/*
 * RpcWrite writes an RPC header to the specified buffer.  We can only
 * send the current protocol version.
 */
static int RpcWrite(NaClSrpcImcBuffer* buffer, NaClSrpcRpc* rpc) {
  dprintf((SIDE "RpcWrite(%"PRIx32", %s, %"PRIu32", %s)\n",
           rpc->protocol_version,
           rpc->is_request ? "request" : "response",
           rpc->rpc_number,
           NaClSrpcErrorString(rpc->app_error)));
  if (1 !=
      __NaClSrpcImcWrite(&rpc->protocol_version,
                         sizeof(rpc->protocol_version),
                         1,
                         buffer)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->request_id,
                         sizeof(rpc->request_id),
                         1,
                         buffer)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->is_request,
                         sizeof(rpc->is_request),
                         1,
                         buffer)) {
    return 0;
  }
  if (1 !=
      __NaClSrpcImcWrite(&rpc->rpc_number,
                         sizeof(rpc->rpc_number),
                         1,
                         buffer)) {
    return 0;
  }
  /* Responses also need to send the app_error member */
  if (!rpc->is_request) {
    if (1 !=
        __NaClSrpcImcWrite(&rpc->app_error,
                           sizeof(rpc->app_error),
                           1,
                           buffer)) {
      return 0;
    }
  }
  dprintf((SIDE "RpcWrite done\n"));
  return 1;
}

static void RpcLength(int is_request, uint32_t* bytes, uint32_t* handles) {
  *bytes =
      sizeof(uint32_t) +  /* protocol_version */
      sizeof(uint64_t) +  /* request_id */
      sizeof(uint8_t)  +  /* is_request */
      sizeof(uint32_t) +  /* rpc_number */
      (is_request ? sizeof(uint32_t) : 0);  /* app_error */
  *handles = 0;
}

/*
 * Deserialize a request from the buffer.  If successful, the input
 * arguments and the template of the returns is returned.
 */
int NaClSrpcRequestGet(NaClSrpcImcBuffer* buffer,
                       const NaClSrpcRpc* rpc,
                       const char* arg_types,
                       NaClSrpcArg* args[],
                       const char* ret_types,
                       NaClSrpcArg* rets[]) {
  const ArgsIoInterface* desc;

  dprintf((SIDE "RequestGet(%p, %"PRIu32"\n",
          (void*) buffer,
          rpc->rpc_number));
  /* Get the Args I/O descriptor for the protocol version read */
  desc = GetArgsInterface(rpc->protocol_version);
  if (!desc->get(desc, buffer, 1, 1, args, arg_types)) {
    dprintf(("RequestGet: argument vector receive failed\n"));
    return 0; /* get frees memory on error. */
  }
  /* Construct the rets from the buffer. */
  if (!desc->get(desc, buffer, 1, 0, rets, ret_types)) {
    dprintf(("RequestGet: rets template receive failed\n"));
    desc->free(desc, args);
    return 0;
  }
  dprintf((SIDE "RequestGet(%p, %"PRIu32") received\n",
           (void*) buffer,
           rpc->rpc_number));
  return 1;
}

static int RequestPut(const ArgsIoInterface* desc,
                      NaClSrpcRpc* rpc,
                      NaClSrpcArg* args[],
                      NaClSrpcArg* rets[],
                      NaClSrpcImcBuffer* buffer) {
  dprintf(("RequestPut(%p, %"PRIu32")\n",
           (void*) buffer,
           rpc->rpc_number));
  /* Set up and send rpc */
  rpc->is_request = 1;
  rpc->app_error = NACL_SRPC_RESULT_OK;
  if (!RpcWrite(buffer, rpc)) {
    return 0;
  }
  /* Then send the args */
  if (!desc->put(desc, buffer, 1, args)) {
    dprintf(("RequestPut: args send failed\n"));
    return 0;
  }
  /* And finally the rets template */
  if (!desc->put(desc, buffer, 0, rets)) {
    dprintf(("RequestPut: rets template send failed\n"));
    return 0;
  }
  dprintf(("RequestPut(%p, %"PRIu32") sent\n",
           (void*) buffer,
           rpc->rpc_number));
  return 1;
}

static int RequestLength(const ArgsIoInterface* desc,
                         NaClSrpcArg* args[],
                         NaClSrpcArg* rets[],
                         uint32_t* bytes,
                         uint32_t* handles) {
  uint32_t tmp_bytes;
  uint32_t tmp_hdl;

  RpcLength(1, &tmp_bytes, &tmp_hdl);
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

int NaClSrpcRequestWrite(NaClSrpcChannel* channel,
                         NaClSrpcRpc* rpc,
                         NaClSrpcArg* args[],
                         NaClSrpcArg* rets[]) {
  uint32_t bytes;
  uint32_t handles;
  const ArgsIoInterface* desc = GetArgsInterface(rpc->protocol_version);
  NaClSrpcImcBuffer* buffer;

  if (!RequestLength(desc, args, rets, &bytes, &handles)) {
    return 0;
  }
  buffer = &channel->send_buf;
  if (!RequestPut(desc, rpc, args, rets, buffer)) {
    return 0;
  }
  if (!__NaClSrpcImcFlush(buffer, channel)) {
    /* Requests with bad handles could fail.  Report to the caller. */
    dprintf(("NaClSrpcRequestWrite(%p, %"PRIu32") failed\n",
             (void*) buffer,
             rpc->rpc_number));
    return 0;
  }
  return 1;
}

/*
 * Deserialize a response from the buffer.  If successful, the return
 * values are returned.
 */
int NaClSrpcResponseGet(NaClSrpcImcBuffer* buffer,
                        const NaClSrpcRpc* rpc,
                        const char* ret_types,
                        NaClSrpcArg* rets[]) {
  const ArgsIoInterface* desc;

  /* Get the Args I/O descriptor for the protocol version read */
  desc = GetArgsInterface(rpc->protocol_version);
  /* Announce start of response processing */
  dprintf((SIDE "ResponseGet: response, rpc %"PRIu32"\n", rpc->rpc_number));
  if (NACL_SRPC_RESULT_OK != rpc->app_error) {
    dprintf(("ResponseGet: method returned failure: %d\n", rpc->app_error));
    return 1;
  }
  dprintf((SIDE "ResponseGet: getting rets\n"));
  if (!desc->get(desc, buffer, 0, 1, rets, ret_types)) {
    dprintf(("ResponseGet: rets receive failed\n"));
    /* get cleans up argument memory before returning */
    return 0;
  }
  dprintf((SIDE "ResponseGet(%p, %"PRIu32") received\n",
           (void*) buffer, rpc->rpc_number));
  return 1;
}

static int ResponsePut(const ArgsIoInterface* desc,
                       NaClSrpcRpc* rpc,
                       NaClSrpcArg* rets[],
                       NaClSrpcImcBuffer* buffer) {
  dprintf((SIDE "ResponsePut(%p, %"PRIu32")\n",
           (void*) buffer, rpc->rpc_number));
  rpc->is_request = 0;
  if (!RpcWrite(buffer, rpc)) {
    return 0;
  }
  if (!desc->put(desc, buffer, 1, rets)) {
    dprintf(("SRPC: rets send failed\n"));
    return 0;
  }
  dprintf((SIDE "ResponsePut(%p, %"PRIu32", %d, %s): sent\n",
           (void*) buffer, rpc->rpc_number, rpc->app_error,
           NaClSrpcErrorString(rpc->app_error)));
  return 1;
}

static int ResponseLength(const ArgsIoInterface* desc,
                          NaClSrpcArg* rets[],
                          uint32_t* bytes,
                          uint32_t* handles) {
  uint32_t tmp_bytes;
  uint32_t tmp_hdl;

  RpcLength(0, &tmp_bytes, &tmp_hdl);
  *bytes += tmp_bytes;
  *handles += tmp_hdl;
  if (!desc->length(desc, rets, 1, &tmp_bytes, &tmp_hdl)) {
    return 0;
  }
  *bytes += tmp_bytes;
  *handles += tmp_hdl;

  return 1;
}

/*
 * NaClSrpcResponseWrite writes the header and the return values on the
 * specified channel.
 */
int NaClSrpcResponseWrite(NaClSrpcChannel* channel,
                          NaClSrpcRpc* rpc,
                          NaClSrpcArg* rets[]) {
  uint32_t bytes;
  uint32_t handles;
  const ArgsIoInterface* desc = GetArgsInterface(rpc->protocol_version);
  NaClSrpcImcBuffer* buffer;

  /*
   * ResponseLength computes the requirements for a write buffer.
   * It is currently unused, but will be used in the next CL to separate
   * serialization from buffer send/receive.
   */
  if (!ResponseLength(desc, rets, &bytes, &handles)) {
    return 0;
  }
  /* Get the buffer to write into */
  buffer = &channel->send_buf;
  /* Serialize into the buffer */
  if (!ResponsePut(desc, rpc, rets, buffer)) {
    dprintf(("ResponseWrite: couldn't put rets\n"));
    return 0;
  }
  /* Flush the buffer to the channel */
  if (!__NaClSrpcImcFlush(buffer, channel)) {
    /*
     * If the flush call fails due to a bad handle, the transport layer
     * doesn't send anything.  Therefore we need to return an error message
     * so that the client doesn't wait forever.
     */
    dprintf(("ResponseWrite: flush failed -- sending internal error\n"));
    rpc->app_error = NACL_SRPC_RESULT_INTERNAL;
    rpc->is_request = 0;
    if (!RpcWrite(buffer, rpc)) {
      dprintf(("ResponseWrite: flush failed twice -- giving up\n"));
      return 0;
    }
    __NaClSrpcImcFlush(buffer, channel);
  }
  dprintf(("NaClSrpcResponseWrite: sent\n"));
  return 1;
}
