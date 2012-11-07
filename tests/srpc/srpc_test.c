/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Interface test for simple rpc (SRPC).  Tests passing of all supported
 * parameter and return types.
 *
 * It really only makes sense to run this test from within either sel_universal
 * or the browser, as it is intended to test marshalling and serialization
 * of parameters.
 */

#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

/*
 * When running embedded, sel_ldr instances are invoked with the -X option.
 * This creates a bound socket on descriptor 3 and its address on descriptor 4.
 * The SRPC system accepts connections on descriptor 3, and creates an SRPC
 * channel with a listener thread for each connection.
 * 4 is a convenient transferrable descriptor for the "handle" parameter type.
 * TODO(sehr): this should move to an SRPC header file.
 */
static const int kSrpcSocketAddressDescriptorNumber = 4;

static const int kInvalidDescriptorNumber = -1;

/*
 *  First, tests for scalar argument passing and return.
 */

/*
 *  The test for bool inverts the input and returns it.
 */
void BoolMethod(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  out_args[0]->u.bval = !in_args[0]->u.bval;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  The test for double negates the input and returns it.
 */
void DoubleMethod(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  out_args[0]->u.dval = -in_args[0]->u.dval;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  The test for returning signalling NaNs takes no input.
 */
void NaNMethod(NaClSrpcRpc *rpc,
               NaClSrpcArg **in_args,
               NaClSrpcArg **out_args,
               NaClSrpcClosure *done) {
  union IntDouble {
    double d;
    int i[2];
  } u;
  u.i[0] = 0x4007ffff;
  u.i[1] = 0xffffffff;
  *((union IntDouble*) (&out_args[0]->u.dval)) = u;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  The test for int negates the input and returns it.
 */
void IntMethod(NaClSrpcRpc *rpc,
               NaClSrpcArg **in_args,
               NaClSrpcArg **out_args,
               NaClSrpcClosure *done) {
  out_args[0]->u.ival = -in_args[0]->u.ival;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  The test for long negates the input and returns it.
 */
void LongMethod(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  out_args[0]->u.lval = -in_args[0]->u.lval;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  The test for string returns the length of the string.
 */
void StringMethod(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  out_args[0]->u.ival = strlen(in_args[0]->arrays.str);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  Second, tests for array argument passing and return.
 *  All the array methods reverse their input arrays and return them.
 *  If the counts don't match, return an error.
 */

void CharArrayMethod(NaClSrpcRpc *rpc,
                     NaClSrpcArg **in_args,
                     NaClSrpcArg **out_args,
                     NaClSrpcClosure *done) {
  int i, length;
  if (out_args[0]->u.count != in_args[0]->u.count) {
    printf("CharArrayMethod: count mismatch: in=%d out=%d\n",
           (int) in_args[0]->u.count, (int) out_args[0]->u.count);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
  }
  length = in_args[0]->u.count;
  for (i = 0; i < length; i++) {
    out_args[0]->arrays.carr[length - i - 1] = in_args[0]->arrays.carr[i];
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void DoubleArrayMethod(NaClSrpcRpc *rpc,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args,
                       NaClSrpcClosure *done) {
  int i, length;
  if (out_args[0]->u.count != in_args[0]->u.count) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
  }
  length = in_args[0]->u.count;
  for (i = 0; i < length; i++) {
    out_args[0]->arrays.darr[length - i - 1] = in_args[0]->arrays.darr[i];
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void IntArrayMethod(NaClSrpcRpc *rpc,
                    NaClSrpcArg **in_args,
                    NaClSrpcArg **out_args,
                    NaClSrpcClosure *done) {
  int i, length;
  if (out_args[0]->u.count != in_args[0]->u.count) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
  }
  length = in_args[0]->u.count;
  for (i = 0; i < length; i++) {
    out_args[0]->arrays.iarr[length - i - 1] = in_args[0]->arrays.iarr[i];
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void LongArrayMethod(NaClSrpcRpc *rpc,
                     NaClSrpcArg **in_args,
                     NaClSrpcArg **out_args,
                     NaClSrpcClosure *done) {
  int i, length;
  if (out_args[0]->u.count != in_args[0]->u.count) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
  }
  length = in_args[0]->u.count;
  for (i = 0; i < length; i++) {
    out_args[0]->arrays.larr[length - i - 1] = in_args[0]->arrays.larr[i];
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * A null RPC to test throughput and latency.
 */
void NullMethod(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * A method to return a string.
 */
void ReturnStringMethod(NaClSrpcRpc *rpc,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args,
                        NaClSrpcClosure *done) {
  static char string[] = "Ich weiss nicht, was soll es bedeuten"
                         "Dass ich so traurig bin,"
                         "Ein Maerchen aus uralten Zeiten,"
                         "Das kommt mir nicht aus dem Sinn."
                         "Die Luft ist kuehl und es dunkelt,"
                         "Und ruhig fliesst der Rhein;"
                         "Der Gipfel des Berges funkelt,"
                         "Im Abendsonnenschein."
                         "Die schoenste Jungfrau sitzet"
                         "Dort oben wunderbar,"
                         "Ihr gold'nes Geschmeide blitzet,"
                         "Sie kaemmt ihr goldenes Haar,"
                         "Sie kaemmt es mit goldenem Kamme,"
                         "Und singt ein Lied dabei;"
                         "Das hat eine wundersame,"
                         "Gewalt'ge Melodei.";
  out_args[0]->arrays.str = strdup(string + in_args[0]->u.ival);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * A method to receive a handle (descriptor).
 */
void HandleMethod(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  /* If we got this far, things succeeded */
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * A method to return a handle (descriptor).
 */
void ReturnHandleMethod(NaClSrpcRpc *rpc,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args,
                        NaClSrpcClosure *done) {
  out_args[0]->u.hval = kSrpcSocketAddressDescriptorNumber;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * A method to accept an invalid handle (descriptor).
 */
void InvalidHandleMethod(NaClSrpcRpc *rpc,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args,
                         NaClSrpcClosure *done) {
  if (kInvalidDescriptorNumber == in_args[0]->u.hval) {
    rpc->result = NACL_SRPC_RESULT_OK;
  } else {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  }
  done->Run(done);
}

/*
 * A method to return an invalid handle (descriptor).
 */
void ReturnInvalidHandleMethod(NaClSrpcRpc *rpc,
                               NaClSrpcArg **in_args,
                               NaClSrpcArg **out_args,
                               NaClSrpcClosure *done) {
  out_args[0]->u.hval = kInvalidDescriptorNumber;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "bool:b:b", BoolMethod },
  { "double:d:d", DoubleMethod },
  { "nan::d", NaNMethod },
  { "int:i:i", IntMethod },
  { "long:l:l", LongMethod },
  { "string:s:i", StringMethod },
  { "char_array:C:C", CharArrayMethod },
  { "double_array:D:D", DoubleArrayMethod },
  { "int_array:I:I", IntArrayMethod },
  { "long_array:L:L", LongArrayMethod },
  { "null_method::", NullMethod },
  { "stringret:i:s", ReturnStringMethod },
  { "handle:h:", HandleMethod },
  { "handleret::h", ReturnHandleMethod },
  { "invalid_handle:h:", InvalidHandleMethod },
  { "invalid_handle_ret::h", ReturnInvalidHandleMethod },
  { NULL, NULL },
};

int main(void) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
