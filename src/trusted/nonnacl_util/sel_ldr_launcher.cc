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

#include "native_client/src/include/portability_io.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include <stdio.h>
#include <sys/types.h>

#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

namespace nacl {

SelLdrLauncher::SelLdrLauncher() :
  child_(kInvalidHandle),
  channel_(kInvalidHandle),
  argc_(-1),
  argv_(NULL),
  is_sel_ldr_(true),
  sock_addr_(NULL) {
}

// Much of the code to connect to sel_ldr instances and open the respective
// SRPC channels is very similar to that in launching and connecting from the
// plugin.
// TODO(sehr): refactor this to remove the duplication.
static NaClDesc *Connect(struct NaClDescEffector *effp,
                         struct NaClDesc *sock_addr) {
  NaClDesc *connected_desc = NULL;

  if (0 == (sock_addr->vtbl->ConnectAddr)(sock_addr, effp)) {
    struct NaClNrdXferEffector *nrd_effp =
        reinterpret_cast<struct NaClNrdXferEffector *>(effp);
    connected_desc = NaClNrdXferEffectorTakeDesc(nrd_effp);
  }
  return connected_desc;
}

static NaClDesc* GetSockAddr(struct NaClDescEffector *effp,
                             struct NaClDesc *desc) {
  NaClImcTypedMsgHdr  header;
  NaClImcMsgIoVec     iovec[1];
  unsigned char       bytes[NACL_ABI_IMC_USER_BYTES_MAX];
  NaClDesc*           descs[NACL_ABI_IMC_USER_DESC_MAX];

  // Set up to receive a message.
  iovec[0].base = bytes;
  iovec[0].length = NACL_ABI_IMC_USER_BYTES_MAX;
  header.iov = iovec;
  header.iov_length = sizeof(iovec) / sizeof(iovec[0]);
  header.ndescv = descs;
  header.ndesc_length = sizeof(descs) / sizeof(descs[0]);
  header.flags = 0;
  // Receive the message.
  NaClImcRecvTypedMessage(desc, effp, &header, 0);
  // Check that there was exactly one descriptor passed.
  if (1 != header.ndesc_length) {
    return NULL;
  }

  return descs[0];
}

bool SelLdrLauncher::OpenSrpcChannels(NaClSrpcChannel* command,
                                      NaClSrpcChannel* untrusted_command,
                                      NaClSrpcChannel* untrusted) {
  int ctor_state = 0;
  int const kCommandCtord = 1;
  int const kUntrustedCommandCtord = 2;
  int const kUntrustedCtord = 4;
  NaClNrdXferEffector eff;
  struct NaClDescEffector *effp = NULL;
  struct NaClDesc *pair[2];
  struct NaClDesc *channel_desc = NULL;
  int start_result;
  struct NaClDesc* command_desc = NULL;
  struct NaClDesc* untrusted_command_desc = NULL;
  struct NaClDesc* untrusted_desc = NULL;
  struct NaClDesc* tmp = NULL;

  pair[0] = NULL;
  pair[1] = NULL;
  // Create a NaClDesc for use by the effector.
  if (0 != NaClCommonDescMakeBoundSock(pair)) {
    goto done;
  }
  if (!NaClNrdXferEffectorCtor(&eff, pair[0])) {
    goto done;
  }
  effp = reinterpret_cast<struct NaClDescEffector *>(&eff);
  // Create a NaClDesc for channel_.
  tmp = reinterpret_cast<struct NaClDesc*>(malloc(sizeof(NaClDescImcDesc)));
  if (NULL == tmp) {
    goto done;
  }
  if (!NaClDescImcDescCtor(
      reinterpret_cast<struct NaClDescImcDesc*>(tmp), channel_)) {
    free(tmp);
    goto done;
  }
  channel_desc = tmp;
  // channel_desc now has ownership of channel_, so we get rid of our
  // "reference" to it.
  channel_ = kInvalidHandle;

  // Get the socket address from the descriptor.
  sock_addr_ = GetSockAddr(effp, channel_desc);
  if (NULL == sock_addr_) {
    goto done;
  }

  // The first connection goes to the trusted command channel.
  if (NULL == (command_desc = Connect(effp, sock_addr_))) {
    goto done;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // NaClSrpcClientCtor takes ownership of command_desc.
  if (!NaClSrpcClientCtor(command, command_desc)) {
    goto done;
  }
  command_desc = NULL;
  ctor_state |= kCommandCtord;
  // Start untrusted code module.
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(command, "start_module", &start_result)) {
    goto done;
  }

  // The second connection goes to the untrusted command channel.
  if (NULL == (untrusted_command_desc = Connect(effp, sock_addr_))) {
    goto done;
    return 0;
  }
  // Start the SRPC client to communicate with the untrusted command channel.
  if (!NaClSrpcClientCtor(untrusted_command, untrusted_command_desc)) {
    goto done;
  }
  untrusted_command_desc = NULL;
  ctor_state |= kUntrustedCommandCtord;

  // The third connection goes to the service itself.
  if (NULL == (untrusted_desc = Connect(effp, sock_addr_))) {
    goto done;
  }
  // Start the SRPC client to communicate with the untrusted service
  if (!NaClSrpcClientCtor(untrusted, untrusted_desc)) {
    goto done;
  }
  untrusted_desc = NULL;
  ctor_state |= kUntrustedCtord;

  NaClDescUnref(channel_desc);
  return true;

 done:
  if (NULL != untrusted_desc) {
    NaClDescUnref(untrusted_desc);
  }
  if (0 != (kUntrustedCommandCtord & ctor_state)) {
    NaClSrpcDtor(untrusted_command);
  }
  if (NULL != untrusted_command_desc) {
    NaClDescUnref(untrusted_command_desc);
  }
  if (0 != (kCommandCtord & ctor_state)) {
    NaClSrpcDtor(command);
  }
  if (NULL != command_desc) {
    NaClDescUnref(command_desc);
  }
  if (NULL != channel_desc) {
    NaClDescUnref(channel_desc);
  }
  if (NULL != effp) {
    (*effp->vtbl->Dtor)(effp);
  }
  if (NULL != pair[0]) {
    NaClDescUnref(pair[0]);
  }
  if (NULL != pair[1]) {
    NaClDescUnref(pair[1]);
  }

  return false;
}

void SelLdrLauncher::BuildArgv(const char* sel_ldr_pathname,
                               const char* application_name,
                               int imc_fd,
                               Handle imc_channel_handle,
                               int sel_ldr_argc,
                               const char* sel_ldr_argv[],
                               int application_argc,
                               const char* application_argv[]) {
  // NOTE: there might be some signedness issues with imc_channel_handle
  SNPRINTF(channel_buf_,
           sizeof(channel_buf_),
           "%d:%u",
           imc_fd,
           imc_channel_handle);
  SNPRINTF(channel_number_, sizeof(channel_number_), "%d", imc_fd);

  // Fixed args are:
  // .../sel_ldr -f <application_name> -i <NaCl fd>:<imcchannel#>
  const char* kFixedArgs[] = {
    const_cast<char*>(sel_ldr_pathname),
    "-f",
    const_cast<char*>(application_name),
    "-i",
    channel_buf_
  };
  const int kFixedArgc = sizeof(kFixedArgs) / sizeof(kFixedArgs[0]);

  argv_ = new char const*[kFixedArgc + sel_ldr_argc
                          + (application_argc + 1) + 1];
  // We use pre-increment everywhere, so we need to initialize to -1.
  argc_ = -1;

  // Copy the fixed arguments to argv_.
  for (int i = 0; i < kFixedArgc; ++i) {
    argv_[++argc_] = kFixedArgs[i];
  }
  // Copy the specified additional arguments (e.g., "-d" or "-P") to sel_ldr
  for (int i = 0; i < sel_ldr_argc; ++i) {
    argv_[++argc_] = sel_ldr_argv[i];
  }
  // Copy the specified arguments to the application
  if (application_argc > 0) {
    argv_[++argc_] = "--";
    for (int i = 0; i < application_argc; ++i) {
      if (strncmp("$CHAN", application_argv[i], 6) == 0) {
        argv_[++argc_] = channel_number_;
      } else {
        argv_[++argc_] = application_argv[i];
      }
    }
  }
  // NULL terminate the argument list.
  argv_[++argc_] = NULL;
}

}  // namespace nacl
