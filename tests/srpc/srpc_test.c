/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
#include <nacl/nacl_srpc.h>

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
NaClSrpcError BoolMethod(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  out_args[0]->u.bval = !in_args[0]->u.bval;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  The test for double negates the input and returns it.
 */
NaClSrpcError DoubleMethod(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  out_args[0]->u.dval = -in_args[0]->u.dval;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  The test for returning signalling NaNs takes no input.
 */
NaClSrpcError NaNMethod(NaClSrpcChannel *channel,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args) {
  union IntDouble {
    double d;
    int i[2];
  } u;
  u.i[0] = 0x4007ffff;
  u.i[1] = 0xffffffff;
  *((union IntDouble*) (&out_args[0]->u.dval)) = u;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  The test for int negates the input and returns it.
 */
NaClSrpcError IntMethod(NaClSrpcChannel *channel,
                        NaClSrpcArg **in_args,
                        NaClSrpcArg **out_args) {
  out_args[0]->u.ival = -in_args[0]->u.ival;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  The test for long negates the input and returns it.
 */
NaClSrpcError LongMethod(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  out_args[0]->u.lval = -in_args[0]->u.lval;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  The test for string returns the length of the string.
 */
NaClSrpcError StringMethod(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  out_args[0]->u.ival = strlen(in_args[0]->u.sval);
  return NACL_SRPC_RESULT_OK;
}

/*
 *  Second, tests for array argument passing and return.
 *  All the array methods reverse their input arrays and return them.
 *  If the counts don't match, return an error.
 */

NaClSrpcError CharArrayMethod(NaClSrpcChannel *channel,
                              NaClSrpcArg **in_args,
                              NaClSrpcArg **out_args) {
  int i, length;
  if (out_args[0]->u.caval.count != in_args[0]->u.caval.count) {
    printf("CharArrayMethod: count mismatch: in=%d out=%d\n",
           (int) in_args[0]->u.caval.count, (int) out_args[0]->u.caval.count);
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  length = in_args[0]->u.caval.count;
  for (i = 0; i < length; i++) {
    out_args[0]->u.caval.carr[length - i - 1] = in_args[0]->u.caval.carr[i];
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError DoubleArrayMethod(NaClSrpcChannel *channel,
                                NaClSrpcArg **in_args,
                                NaClSrpcArg **out_args) {
  int i, length;
  if (out_args[0]->u.daval.count != in_args[0]->u.daval.count) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  length = in_args[0]->u.daval.count;
  for (i = 0; i < length; i++) {
    out_args[0]->u.daval.darr[length - i - 1] = in_args[0]->u.daval.darr[i];
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError IntArrayMethod(NaClSrpcChannel *channel,
                             NaClSrpcArg **in_args,
                             NaClSrpcArg **out_args) {
  int i, length;
  if (out_args[0]->u.iaval.count != in_args[0]->u.iaval.count) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  length = in_args[0]->u.iaval.count;
  for (i = 0; i < length; i++) {
    out_args[0]->u.iaval.iarr[length - i - 1] = in_args[0]->u.iaval.iarr[i];
  }
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError LongArrayMethod(NaClSrpcChannel *channel,
                              NaClSrpcArg **in_args,
                              NaClSrpcArg **out_args) {
  int i, length;
  if (out_args[0]->u.laval.count != in_args[0]->u.laval.count) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  length = in_args[0]->u.laval.count;
  for (i = 0; i < length; i++) {
    out_args[0]->u.laval.larr[length - i - 1] = in_args[0]->u.laval.larr[i];
  }
  return NACL_SRPC_RESULT_OK;
}

/*
 * A null RPC to test throughput and latency.
 */
NaClSrpcError NullMethod(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  return NACL_SRPC_RESULT_OK;
}

/*
 * A method to return a string.
 */
NaClSrpcError ReturnStringMethod(NaClSrpcChannel *channel,
                                 NaClSrpcArg **in_args,
                                 NaClSrpcArg **out_args) {
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
  out_args[0]->u.sval = strdup(string + in_args[0]->u.ival);
  return NACL_SRPC_RESULT_OK;
}

/*
 * A method to receive a handle (descriptor).
 */
NaClSrpcError HandleMethod(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  /* If we got this far, things succeeded */
  return NACL_SRPC_RESULT_OK;
}

/*
 * A method to return a handle (descriptor).
 */
NaClSrpcError ReturnHandleMethod(NaClSrpcChannel *channel,
                                 NaClSrpcArg **in_args,
                                 NaClSrpcArg **out_args) {
  out_args[0]->u.hval = kSrpcSocketAddressDescriptorNumber;
  return NACL_SRPC_RESULT_OK;
}

/*
 * A method to accept an invalid handle (descriptor).
 */
NaClSrpcError InvalidHandleMethod(NaClSrpcChannel *channel,
                                  NaClSrpcArg **in_args,
                                  NaClSrpcArg **out_args) {
  if (kInvalidDescriptorNumber == in_args[0]->u.hval) {
    return NACL_SRPC_RESULT_OK;
  } else {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
}

/*
 * A method to return an invalid handle (descriptor).
 */
NaClSrpcError ReturnInvalidHandleMethod(NaClSrpcChannel *channel,
                                        NaClSrpcArg **in_args,
                                        NaClSrpcArg **out_args) {
  out_args[0]->u.hval = kInvalidDescriptorNumber;
  return NACL_SRPC_RESULT_OK;
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

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
