/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_

#include "native_client/src/shared/srpc/nacl_srpc.h"

/**
 * A utility type used by NaClSrpcCommandLoop.
 */
typedef NaClSrpcError (*NaClSrpcInterpreter)(NaClSrpcService* service,
                                             NaClSrpcChannel* channel,
                                             uint32_t rpc_number,
                                             NaClSrpcArg** ins,
                                             NaClSrpcArg** outs);

/**
 * The interpreter loop used by sel_universal and non-embedded applications.
 * It runs a loop that reads from stdin, and invokes the specified command
 * interpreter.  As the interpreter may need to know either the service or
 * channel on which it needs to run, these are also specified.  To provide
 * access to at least one valid transferrable descriptor, we pass the
 * default socket address descriptor for the NaCl module.
 */
extern void NaClSrpcCommandLoop(NaClSrpcService* service,
                                NaClSrpcChannel* channel,
                                NaClSrpcInterpreter interpreter,
                                NaClSrpcImcDescType default_socket_address);


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_RPC_UNIVERSAL_H_ */
