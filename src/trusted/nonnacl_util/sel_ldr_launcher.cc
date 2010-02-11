/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include <stdio.h>
#include <sys/types.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

namespace nacl {

SelLdrLauncher::SelLdrLauncher()
    : child_(kInvalidHandle),
      channel_(kInvalidHandle),
      argc_(-1),
      argv_(NULL),
      is_sel_ldr_(true),
      sock_addr_(NULL) {
}

static DescWrapper* GetSockAddr(DescWrapper* desc) {
  DescWrapper::MsgHeader   header;
  DescWrapper::MsgIoVec    iovec[1];
  unsigned char            bytes[NACL_ABI_IMC_USER_BYTES_MAX];
  DescWrapper*             descs[NACL_ABI_IMC_USER_DESC_MAX];

  // Set up to receive a message.
  iovec[0].base = bytes;
  iovec[0].length = NACL_ABI_IMC_USER_BYTES_MAX;
  header.iov = iovec;
  header.iov_length = NACL_ARRAY_SIZE(iovec);
  header.ndescv = descs;
  header.ndescv_length = NACL_ARRAY_SIZE(descs);
  header.flags = 0;
  // Receive the message.
  if (0 != desc->RecvMsg(&header, 0)) {
    return NULL;
  }
  // Check that there was exactly one descriptor passed.
  if (1 != header.ndescv_length) {
    return NULL;
  }

  return descs[0];
}

bool SelLdrLauncher::OpenSrpcChannels(NaClSrpcChannel* command,
                                      NaClSrpcChannel* untrusted_command,
                                      NaClSrpcChannel* untrusted) {
  int start_result;
  bool retval = false;
  DescWrapper* channel_desc = NULL;
  DescWrapper* sock_addr = NULL;
  DescWrapper* command_desc = NULL;
  DescWrapper* untrusted_command_desc = NULL;
  DescWrapper* untrusted_desc = NULL;

  DescWrapperFactory factory;
  channel_desc = factory.MakeImcSock(channel_);
  if (NULL == channel_desc) {
    goto done;
  }
  // channel_desc now has ownership of channel_, so we get rid of our
  // "reference" to it.
  channel_ = kInvalidHandle;

  // Get the socket address from the descriptor.
  sock_addr = GetSockAddr(channel_desc);
  if (NULL == sock_addr) {
    NaClLog(4, "SelLdrLauncher::OpenSrpcChannels: GetSockAddr failed\n");
    goto done;
  }
  // Save the socket address for other uses.
  sock_addr_ = NaClDescRef(sock_addr->desc());

  // The first connection goes to the trusted command channel.
  command_desc = sock_addr->Connect();
  if (NULL == command_desc) {
    NaClLog(4, "SelLdrLauncher::OpenSrpcChannels: Connect failed\n");
    goto done;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes ownership of command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    goto done;
  }

  // Start untrusted code module.
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeByName(command, "start_module", &start_result)) {
    goto done;
  }

  // The second connection goes to the untrusted command channel.
  untrusted_command_desc = sock_addr->Connect();
  if (NULL == untrusted_command_desc) {
    goto done;
  }
  // Start the SRPC client to communicate with the untrusted command channel.
  // SRPC client takes ownership of untrusted_command_desc.
  if (!NaClSrpcClientCtor(untrusted_command, untrusted_command_desc->desc())) {
    goto done;
  }

  // The third connection goes to the service itself.
  untrusted_desc = sock_addr->Connect();
  if (NULL == untrusted_desc) {
    goto done;
  }
  // Start the SRPC client to communicate with the untrusted service
  // SRPC client takes ownership of untrusted_desc.
  if (!NaClSrpcClientCtor(untrusted, untrusted_desc->desc())) {
    goto done;
  }
  retval = true;

 done:
  DescWrapper::SafeDelete(command_desc);
  DescWrapper::SafeDelete(untrusted_command_desc);
  DescWrapper::SafeDelete(untrusted_desc);
  DescWrapper::SafeDelete(channel_desc);
  DescWrapper::SafeDelete(sock_addr);

  return retval;
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
  const int kFixedArgc = NACL_ARRAY_SIZE(kFixedArgs);

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
