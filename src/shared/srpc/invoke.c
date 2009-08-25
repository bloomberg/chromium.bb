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
 * NaCl service library.  a primitive rpc library
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifdef __native_client__
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#else
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#endif  /* __native_client__ */
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"


static int GetRpcTypes(NaClSrpcChannel* channel,
                       uint32_t rpc_num,
                       const char** in_types,
                       const char** out_types) {
  if (0 == rpc_num) {
    *in_types = "";
    *out_types = "C";
    return 1;
  } else if (NACL_SRPC_GET_TIMES_METHOD == rpc_num) {
    *in_types = "";
    *out_types = "dddd";
    return 1;
  } else if (NACL_SRPC_TOGGLE_CHANNEL_TIMING_METHOD == rpc_num) {
    *in_types = "i";
    *out_types = "";
    return 1;
  } else if (channel->rpc_count > rpc_num) {
    *in_types = channel->rpc_descr[rpc_num].in_args;
    *out_types = channel->rpc_descr[rpc_num].out_args;
    return 1;
  } else {
    return 0;
  }
}

/*
 * Methods for invoking RPCs.
 */
NaClSrpcError NaClSrpcInvokeV(NaClSrpcChannel* channel,
                              uint32_t rpc_number,
                              NaClSrpcArg* args[],
                              NaClSrpcArg* rets[]) {
  NaClSrpcRpc      rpc;
  NaClSrpcError    retval;
  int              ret;
  const char*      arg_types;
  const char*      ret_types;
  double           this_start_usec = 0.0;
  double           this_method_usec;

  dprintf(("CLIENT: channel %p, rpc number %d\n", (void*) channel,
           (unsigned) rpc_number));
  /*
   * If we are timing, get the start time.
   */
  dprintf(("CLIENT: channel timing %s\n",
          ((channel->timing_enabled != 0) ? "ENABLED" : "DISABLED")));
  if (channel->timing_enabled) {
    this_start_usec = __NaClSrpcGetUsec();
  }

  if (GetRpcTypes(channel, rpc_number, &arg_types, &ret_types)) {
    /* Check input parameters for type conformance */
    if (0 == NaClSrpcTypeCheckOne(&arg_types, args)) {
      return NACL_SRPC_RESULT_IN_ARG_TYPE_MISMATCH;
    }
    /* Check return values for type conformance */
    if (0 == NaClSrpcTypeCheckOne(&ret_types, rets)) {
      return NACL_SRPC_RESULT_OUT_ARG_TYPE_MISMATCH;
    }
  } else {
    return NACL_SRPC_RESULT_BAD_RPC_NUMBER;
  }

  /*
   * First we send the request.
   * This requires sending args and the types and array sizes from rets.
   */
  rpc.rpc_number = rpc_number;
  rpc.request_id = channel->next_message_id;
  retval = NaClSrpcSendRequest(channel, &rpc, args, rets);
  if (NACL_SRPC_RESULT_OK != retval) {
    dprintf(("SRPC: rpc request send failed\n"));
    return retval;
  }

  /*
   * Then we wait for the response.
   * This requires reading an error code reported by the rpc service itself.
   * If that error code is ok, then we get the return value vector.
   */
  ret = NaClSrpcReceiveMessage(channel, &rpc, args, rets);
  if (1 != ret) {
    dprintf(("SRPC: response receive failed\n"));
    return NACL_SRPC_RESULT_INTERNAL;
  }
  if (0 != rpc.is_request) {
    dprintf(("SRPC: rpc is not response: %d\n", rpc.is_request));
    /* Clear the rest of the buffer contents to ensure alignment */
    __NaClSrpcImcMarkReadBufferEmpty(channel);
    return NACL_SRPC_RESULT_INTERNAL;
  }
  dprintf(("SRPC: received response\n"));

  /*
   * If we are timing, collect the current time, compute the delta from
   * the start, and update the cumulative counter.
   */
  if (0 != channel->timing_enabled) {
    this_method_usec = __NaClSrpcGetUsec();
    channel->send_usec += this_method_usec;
  }

  return rpc.app_error;
}

/*
 * TODO(sehr): split this function.  It's much too big.
 */
NaClSrpcError NaClSrpcInvokeVaList(NaClSrpcChannel  *channel,
                                   uint32_t         rpc_num,
                                   va_list          in_va,
                                   va_list          out_va) {
  char const        *in_arglist;
  char const        *out_arglist;
  size_t            num_in;
  size_t            num_out;
  uint32_t          i;
  int               valid_rpc;
  NaClSrpcArg       **inv;
  NaClSrpcArg       **outv;
  char const        *p;
  NaClSrpcError     rv;

  valid_rpc = GetRpcTypes(channel, rpc_num, &in_arglist, &out_arglist);
  if (0 == valid_rpc) {
    return NACL_SRPC_RESULT_BAD_RPC_NUMBER;
  }

  num_in = strlen(in_arglist);
  num_out = strlen(out_arglist);

  if (NACL_SRPC_MAX_ARGS < num_in || NACL_SRPC_MAX_ARGS < num_out) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }

  rv = NACL_SRPC_RESULT_NO_MEMORY;
  inv = (NaClSrpcArg **) malloc((num_in + 1) * sizeof *inv);
  if (inv == NULL) {
    goto abort0;
  }
  if (0 != num_in) {
    inv[0] = (NaClSrpcArg *) malloc(num_in * sizeof *inv[0]);
    if (NULL == inv[0]) {
      goto abort1;
    }
    for (i = 1; i < num_in; ++i) {
      inv[i] = inv[0] + i;
    }
  }
  inv[num_in] = 0;

  outv = (NaClSrpcArg **) malloc((num_out+1) * sizeof *outv);
  if (NULL == outv) {
    goto abort2;
  }
  if (0 != num_out) {
    outv[0] = (NaClSrpcArg *) malloc(num_out * sizeof *outv[0]);
    if (NULL == outv[0]) {
      goto abort3;
    }
    for (i = 1; i < num_out; ++i) {
      outv[i] = outv[0] + i;
    }
  }
  outv[num_out] = 0;

  rv = NACL_SRPC_RESULT_INTERNAL;

  for (i = 0, p = in_arglist; i < num_in; ++i, ++p) {
    switch (*p) {
      case NACL_SRPC_ARG_TYPE_BOOL:
        inv[i]->u.bval = va_arg(in_va, int) != 0;
        break;
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        inv[i]->u.caval.count = va_arg(in_va, uint32_t);
        inv[i]->u.caval.carr = va_arg(in_va, char *);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        inv[i]->u.dval = va_arg(in_va, double);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        inv[i]->u.daval.count = va_arg(in_va, uint32_t);
        inv[i]->u.daval.darr = va_arg(in_va, double *);
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        inv[i]->u.hval = va_arg(in_va, NaClSrpcImcDescType);
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        inv[i]->u.ival = va_arg(in_va, int);
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        inv[i]->u.iaval.count = va_arg(in_va, uint32_t);
        inv[i]->u.iaval.iarr = va_arg(in_va, int *);
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
        /*
         * When args are actually passed via a read/write
         * interface, this essentially turns into a counted
         * string, except that we count the bytes.
         */
        inv[i]->u.sval = va_arg(in_va, char *);
        break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        rv = NACL_SRPC_RESULT_APP_ERROR;
        goto abort4;
    }
    inv[i]->tag = *p;
  }

  for (i = 0, p = out_arglist; i < num_out; ++i, ++p) {
    switch (*p) {
      case NACL_SRPC_ARG_TYPE_BOOL:
        outv[i]->u.bval = 0;
        (void) va_arg(in_va, int *);
        break;
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        outv[i]->u.caval.count = va_arg(in_va, uint32_t);
        outv[i]->u.caval.carr = va_arg(in_va, char *);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        outv[i]->u.dval = 0;
        (void) va_arg(in_va, double *);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        outv[i]->u.daval.count = va_arg(in_va, uint32_t);
        outv[i]->u.daval.darr = va_arg(in_va, double *);
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        outv[i]->u.hval = 0;
        (void) va_arg(in_va, NaClSrpcImcDescType);
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        outv[i]->u.ival = 0;
        (void) va_arg(in_va, int *);
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        outv[i]->u.iaval.count = va_arg(in_va, uint32_t);
        outv[i]->u.iaval.iarr = va_arg(in_va, int *);
        break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        rv = NACL_SRPC_RESULT_APP_ERROR;
        goto abort4;
    }
    outv[i]->tag = *p;
  }

  rv = NaClSrpcInvokeV(channel, rpc_num, inv, outv);

  for (i = 0, p = in_arglist; i < num_in; ++i, ++p) {
    switch (*p) {
      case NACL_SRPC_ARG_TYPE_BOOL:
        (void) va_arg(out_va, int);
        break;
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, char *);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        (void) va_arg(out_va, double);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, double *);
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        (void) va_arg(out_va, void *);
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        (void) va_arg(out_va, int);
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, int *);
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
        (void) va_arg(out_va, char *);
        break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        rv = NACL_SRPC_RESULT_APP_ERROR;
        goto abort4;
    }
  }
  for (i = 0, p = out_arglist; i < num_out; ++i, ++p) {
    switch (*p) {
      case NACL_SRPC_ARG_TYPE_BOOL:
        *va_arg(out_va, int *) = outv[i]->u.bval != 0;
        break;
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, char *);
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE:
        *va_arg(out_va, double *) = outv[i]->u.dval;
        break;
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, double *);
        break;
      case NACL_SRPC_ARG_TYPE_HANDLE:
        *va_arg(out_va, NaClSrpcImcDescType *) = outv[i]->u.hval;
        break;
      case NACL_SRPC_ARG_TYPE_INT:
        *va_arg(out_va, int *) = outv[i]->u.ival;
        break;
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
        (void) va_arg(out_va, uint32_t);
        (void) va_arg(out_va, int *);
        break;
      case NACL_SRPC_ARG_TYPE_STRING:
        /* fall through; earlier checks should have caught this */
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        rv = NACL_SRPC_RESULT_APP_ERROR;
        goto abort4;
    }
  }

abort4:
  free(outv[0]);
abort3:
  free(outv);
abort2:
  free(inv[0]);
abort1:
  free(inv);
abort0:
  return rv;
}

NaClSrpcError NaClSrpcInvoke(NaClSrpcChannel  *channel,
                             uint32_t         rpc_num,
                             ...) {
  va_list       in_va;
  va_list       out_va;
  NaClSrpcError rv;

  va_start(in_va, rpc_num);
  va_start(out_va, rpc_num);

  rv = NaClSrpcInvokeVaList(channel, rpc_num, in_va, out_va);

  va_end(out_va);
  va_end(in_va);

  return rv;
}

NaClSrpcError NaClSrpcInvokeByName(NaClSrpcChannel  *channel,
                                   const char       *rpc_name,
                                   ...) {
  int                 rpc_num;
  va_list             in_va;
  va_list             out_va;
  NaClSrpcError       rv;

  rpc_num = NaClSrpcGetRpcNum(channel, rpc_name);
  /* -1 handled by unsigned compare */

  va_start(in_va, rpc_name);
  va_start(out_va, rpc_name);

  rv = NaClSrpcInvokeVaList(channel, (uint32_t) rpc_num, in_va, out_va);

  va_end(out_va);
  va_end(in_va);

  return rv;
}
