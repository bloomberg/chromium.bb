/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "native_client/src/shared/ldr/nacl_ldr.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_message.h"

#ifdef __native_client__
#include <errno.h>
#include "native_client/src/public/imc_syscalls.h"
#else
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#endif


/*
 * Defines for the executable portions of the differences between trusted
 * and untrusted code.
 */
#ifdef __native_client__

static const NaClSrpcImcDescType  kInvalidDesc = -1;

static int ImcConnectAddr(NaClSrpcImcDescType desc,
                          NaClSrpcImcDescType *out_desc) {
  int retval = imc_connect(desc);
  if (-1 == retval) {
    return -errno;
  }
  *out_desc = retval;
  return 0;
}

#else  /* trusted code */

static const NaClSrpcImcDescType  kInvalidDesc = NULL;

static int ImcConnectAddr(NaClSrpcImcDescType desc,
                          NaClSrpcImcDescType *out_desc) {
  return NACL_VTBL(NaClDesc, desc)->ConnectAddr(desc, out_desc);
}

#endif  /* __native_client__ */

int NaClLdrSetupCommandChannel(NaClSrpcImcDescType     socket_addr,
                               struct NaClSrpcChannel  *command_channel) {
  NaClSrpcImcDescType   command_desc = kInvalidDesc;
  int                   retval;

  NaClLog(4,
          ("NaClLdrSetupCommandChannel(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) socket_addr,
          (uintptr_t) command_channel);

  /* The first connection goes to the trusted command channel. */
  retval = ImcConnectAddr(socket_addr, &command_desc);
  if (0 != retval) {
    NaClLog(LOG_ERROR,
            ("NaClLdrSetup: Connect failed with error %d\n"),
            retval);
    return 0;
  }

  NaClLog(4,
          ("NaClLdrSetupCommandChannel:"
           "connected descriptor 0x%08"NACL_PRIxPTR"\n"),
          (uintptr_t) command_desc);

  /* To make sure it's safe to call NaClSrpcDtor on the channel later. */
  NaClSrpcChannelInitialize(command_channel);

  /*
   * Start the SRPC client to communicate with the trusted command channel.
   * SRPC client takes an additional reference to command_desc.
   */
  if (!NaClSrpcClientCtor(command_channel, command_desc)) {
    NaClLog(0, "NaClSelLdrSetupCommandChannel: NaClSrpcClientCtor failed\n");
    return 0;
  }

  return 1;
}

int NaClLdrLoadModule(struct NaClSrpcChannel  *command_channel,
                      NaClSrpcImcDescType     nexe) {
  /* TODO(phosek): This argument to load_module is unused.  Remove it. */
  static const char kLoadModulePlaceHolderString[] = "place holder";
  NaClSrpcError     rpc_result;

  NaClLog(4,
          ("NaClLdrLoadModule(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) command_channel,
          (uintptr_t) nexe);

  CHECK(nexe != kInvalidDesc);

  /* Load nexe module. */
  rpc_result = NaClSrpcInvokeBySignature(command_channel,
                                         "load_module:hs:",
                                         nexe,
                                         kLoadModulePlaceHolderString);
  NaClLog(4, "NaClLdrLoadModule: load_module RPC result %d\n",
          (int) rpc_result);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClLog(LOG_ERROR,
            "NaClLdrLoadModule: load_module failed: rpc_result=%d\n",
            (int) rpc_result);
    NaClSrpcDtor(command_channel);
    return 0;
  }

  return 1;
}

int NaClLdrSetupReverseSetup(struct NaClSrpcChannel   *command_channel,
                             NaClSrpcImcDescType      *reverse_addr) {
  NaClSrpcError         rpc_result;

  NaClLog(4,
          ("NaClLdrSetupReverseSetup(0x%08"NACL_PRIxPTR", 0x%08"
           NACL_PRIxPTR")\n"),
          (uintptr_t) command_channel,
          (uintptr_t) reverse_addr);

  /* Hook up the reverse service channel. */
  rpc_result = NaClSrpcInvokeBySignature(command_channel,
                                         "reverse_setup::h",
                                         reverse_addr);
  NaClLog(4, "NaClLdrSetupReverseSetup: reverse_setup RPC result %d\n",
          (int) rpc_result);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClLog(LOG_ERROR,
            ("NaClLdrSetupReverseSetup: "
            "reverse_setup failed: rpc_result=%d, 0x%08"NACL_PRIxPTR"\n"),
            (int) rpc_result, (uintptr_t) reverse_addr);
    NaClSrpcDtor(command_channel);
    return 0;
  }

  return 1;
}

int NaClLdrStartModule(struct NaClSrpcChannel *command_channel) {
  NaClSrpcError   rpc_result;
  int             start_result;

  /* Start untrusted code module. */
  rpc_result = NaClSrpcInvokeBySignature(command_channel,
                                         "start_module::i",
                                         &start_result);
  NaClLog(4, "NaClLdrStartModule: start_module RPC result %d\n",
          (int) rpc_result);
  if (NACL_SRPC_RESULT_OK != rpc_result || 0 != start_result) {
    NaClLog(LOG_ERROR,
            ("NaClLdrStartModule: "
            "start_module failed: rpc_result=%d, start_result=%d\n"),
            (int) rpc_result, start_result);
    NaClSrpcDtor(command_channel);
    return 0;
  }

  return 1;
}

int NaClLdrSetupUntrustedChannel(NaClSrpcImcDescType     socket_addr,
                                 struct NaClSrpcChannel  *untrusted_channel) {
  NaClSrpcImcDescType   untrusted_desc = kInvalidDesc;
  int                   retval;

  NaClLog(4,
          ("NaClLdrSetupCommandChannel(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) socket_addr,
          (uintptr_t) untrusted_channel);

  /* The second connection goes to the untrusted service itself. */
  retval = ImcConnectAddr(socket_addr, &untrusted_desc);
  if (0 != retval) {
    NaClLog(4,
            ("NaClLdrSetupApplicationChannel: "
            "Connect failed with error %d\n"),
            retval);
    return 0;
  }

  NaClLog(4,
          ("NaClLdrSetupUntrustedChannel:"
           "connected descriptor 0x%08"NACL_PRIxPTR"\n"),
          (uintptr_t) untrusted_desc);

  /* To make sure it's safe to call NaClSrpcDtor on the channel later. */
  NaClSrpcChannelInitialize(untrusted_channel);

  /*
   * Start the SRPC client to communicate with the untrusted service
   * SRPC client takes an additional reference to untrusted_desc.
   */
  if (!NaClSrpcClientCtor(untrusted_channel, untrusted_desc)) {
    NaClLog(4,
            ("NaClLdrSetupUntrustedChannel: "
            "NaClSrpcClientCtor failed\n)"));
    return 0;
  }

  return 1;
}
