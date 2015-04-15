/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/public/secure_service.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


namespace nacl {

SelLdrLauncherBase::SelLdrLauncherBase()
  : channel_(NACL_INVALID_HANDLE),
    bootstrap_socket_(NULL),
    secure_socket_addr_(NULL),
    socket_addr_(NULL) {
}

SelLdrLauncherBase::~SelLdrLauncherBase() {
  if (NACL_INVALID_HANDLE != channel_) {
    NaClClose(channel_);
  }
}

bool SelLdrLauncherBase::ConnectBootstrapSocket() {
  CHECK(factory_ == NULL);
  factory_.reset(new DescWrapperFactory);
  CHECK(channel_ != NACL_INVALID_HANDLE);
  bootstrap_socket_.reset(factory_->MakeImcSock(channel_));
  if (bootstrap_socket_ == NULL) {
    return false;
  }
  // bootstrap_socket_ now has ownership of channel_, so we get rid of
  // our "reference" to it.
  channel_ = NACL_INVALID_HANDLE;

  return true;
}

bool SelLdrLauncherBase::RetrieveSockAddr() {
  DescWrapper::MsgHeader   header;
  DescWrapper::MsgIoVec    iovec[1];
  DescWrapper*             descs[NACL_ABI_IMC_USER_DESC_MAX];
  scoped_array<unsigned char> bytes(
      new unsigned char[NACL_ABI_IMC_USER_BYTES_MAX]);
  if (bytes.get() == NULL) {
    return false;
  }

  // Set up to receive a message.
  iovec[0].base = bytes.get();
  iovec[0].length = NACL_ABI_IMC_USER_BYTES_MAX;
  header.iov = iovec;
  header.iov_length = NACL_ARRAY_SIZE(iovec);
  header.ndescv = descs;
  header.ndescv_length = NACL_ARRAY_SIZE(descs);
  header.flags = 0;
  // Receive the message.
  ssize_t received = bootstrap_socket_->RecvMsg(&header, 0, NULL);
  if (0 != received) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::RetrieveSockAddr: "
            "RecvMsg() returned %d\n", static_cast<int>(received));
    return false;
  }
  // Check that there were exactly two descriptors passed.
  if (2 != header.ndescv_length) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::RetrieveSockAddr: "
            "got desc count %d, expected 2\n",
            static_cast<int>(header.ndescv_length));
    return false;
  }
  secure_socket_addr_.reset(descs[0]);
  socket_addr_.reset(descs[1]);

  return true;
}

bool SelLdrLauncherBase::SetupCommand(NaClSrpcChannel* command) {
  // Get the bootstrap socket.
  if (!ConnectBootstrapSocket()) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupCommand: "
            "getting bootstrap socket failed\n");
    return false;
  }
  // Get the socket address from the descriptor.
  if (!RetrieveSockAddr()) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupCommand: "
            "getting sel_ldr socket address failed\n");
    return false;
  }
  // Connect to the trusted command channel.
  scoped_ptr<DescWrapper> command_desc(secure_socket_addr_->Connect());
  if (command_desc == NULL) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupCommand: Connect() failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes an additional reference to command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupCommand: "
            "NaClSrpcClientCtor failed\n");
    return false;
  }
  return true;
}

bool SelLdrLauncherBase::LoadModule(NaClSrpcChannel* command,
                                    DescWrapper* nexe) {
  CHECK(nexe != NULL);
  // Load module over command channel.
  NaClSrpcResultCodes rpc_result =
      NaClSrpcInvokeBySignature(command,
                                NACL_SECURE_SERVICE_LOAD_MODULE,
                                nexe->desc());
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::LoadModule: "
            "rpc_result=%d is not successful\n",
            static_cast<int>(rpc_result));
    NaClSrpcDtor(command);
    return false;
  }
  return true;
}

bool SelLdrLauncherBase::StartModule(NaClSrpcChannel* command) {
  // Start untrusted code module.
  int start_result;
  NaClSrpcResultCodes rpc_result = NaClSrpcInvokeBySignature(
      command,
      NACL_SECURE_SERVICE_START_MODULE,
      &start_result);
  NaClLog(4, "SelLdrLauncherBase::StartModule rpc result %d\n",
          static_cast<int>(rpc_result));
  if (NACL_SRPC_RESULT_OK != rpc_result || LOAD_OK != start_result) {
    NaClSrpcDtor(command);
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::StartModule: "
            "start_module failed: rpc_result=%d, start_result=%d (%s)\n",
            static_cast<int>(rpc_result), start_result,
            NaClErrorString(static_cast<NaClErrorCode>(start_result)));
    return false;
  }
  return true;
}

bool SelLdrLauncherBase::SetupAppChannel(NaClSrpcChannel* out_app_chan) {
  // Connect to the untrusted service itself.
  scoped_ptr<DescWrapper> untrusted_desc(socket_addr_->Connect());
  if (untrusted_desc == NULL) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupAppChannel: "
            "Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the untrusted service
  // SRPC client takes an additional reference to untrusted_desc.
  if (!NaClSrpcClientCtor(out_app_chan, untrusted_desc->desc())) {
    NaClLog(LOG_ERROR, "SelLdrLauncherBase::SetupAppChannel: "
            "NaClSrpcClientCtor failed\n");
    return false;
  }
  return true;
}

}  // namespace nacl
