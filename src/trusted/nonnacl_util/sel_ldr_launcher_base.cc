/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


namespace nacl {

SelLdrLauncherBase::SelLdrLauncherBase()
  : channel_(kInvalidHandle),
    bootstrap_socket_(NULL),
    socket_addr_(NULL) {
}

SelLdrLauncherBase::~SelLdrLauncherBase() {
  if (kInvalidHandle != channel_) {
    Close(channel_);
  }
}

static DescWrapper* GetSockAddr(DescWrapper* desc) {
  DescWrapper::MsgHeader   header;
  DescWrapper::MsgIoVec    iovec[1];
  DescWrapper*             descs[NACL_ABI_IMC_USER_DESC_MAX];
  scoped_array<unsigned char> bytes(
      new unsigned char[NACL_ABI_IMC_USER_BYTES_MAX]);
  if (bytes.get() == NULL) {
    return NULL;
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
  if (0 != desc->RecvMsg(&header, 0, NULL)) {
    return NULL;
  }
  // Check that there was exactly one descriptor passed.
  if (1 != header.ndescv_length) {
    return NULL;
  }

  return descs[0];
}

bool SelLdrLauncherBase::SetupCommandAndLoad(NaClSrpcChannel* command,
                                             DescWrapper* nexe) {
  // Get the bootstrap socket.
  CHECK(factory_ == NULL);
  factory_.reset(new DescWrapperFactory);
  CHECK(channel_ != kInvalidHandle);
  bootstrap_socket_.reset(factory_->MakeImcSock(channel_));
  if (bootstrap_socket_ == NULL) {
    NaClLog(4, ("SelLdrLauncher::SetupCommandAndLoad: "
                "getting bootstrap socket failed\n"));
    return false;
  }
  // bootstrap_socket_ now has ownership of channel_, so we get rid of
  // our "reference" to it.
  channel_ = kInvalidHandle;
  // Get the socket address from the descriptor.
  socket_addr_.reset(GetSockAddr(bootstrap_socket_.get()));
  if (socket_addr_ == NULL) {
    NaClLog(0, "SelLdrLauncher::SetupCommandAndLoad: "
            "getting sel_ldr socket address failed\n");
    return false;
  }
  // The first connection goes to the trusted command channel.
  scoped_ptr<DescWrapper> command_desc(socket_addr_->Connect());
  if (command_desc == NULL) {
    NaClLog(0, "SelLdrLauncher::SetupCommandAndLoad: Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes an additional reference to command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    NaClLog(0, "SelLdrLauncher::SetupCommandAndLoad: "
            "NaClSrpcClientCtor failed\n");
    return false;
  }
  if (NULL != nexe) {
    // TODO(sehr): This argument to load_module is unused.  Remove it.
    static const char kLoadModulePlaceHolderString[] = "place holder";
    NaClSrpcResultCodes rpc_result =
        NaClSrpcInvokeBySignature(command,
                                  "load_module:hs:",
                                  nexe->desc(),
                                  kLoadModulePlaceHolderString);
    if (NACL_SRPC_RESULT_OK != rpc_result) {
      NaClLog(0, "SelLdrLauncher::SetupCommandAndLoad: "
              "rpc_result= %d is not successful\n",
              static_cast<int>(rpc_result));
      NaClSrpcDtor(command);
      return false;
    }
  }
  return true;
}

bool
SelLdrLauncherBase::StartModuleAndSetupAppChannel(
    NaClSrpcChannel* command,
    NaClSrpcChannel* out_app_chan) {
  // Start untrusted code module.
  int start_result;
  NaClSrpcResultCodes rpc_result = NaClSrpcInvokeBySignature(command,
                                                             "start_module::i",
                                                             &start_result);
  NaClLog(4, "SelLdrLauncher::StartModule rpc result %d\n",
          static_cast<int>(rpc_result));
  if (NACL_SRPC_RESULT_OK != rpc_result && LOAD_OK != start_result) {
    NaClSrpcDtor(command);
    NaClLog(4, "SelLdrLauncher::StartModuleAndSetupAppChannel: "
            "start_module failed: rpc_result=%d, start_result=%d (%s)\n",
            static_cast<int>(rpc_result), start_result,
            NaClErrorString(static_cast<NaClErrorCode>(start_result)));
    return false;
  }
  // The second connection goes to the untrusted service itself.
  scoped_ptr<DescWrapper> untrusted_desc(socket_addr_->Connect());
  if (untrusted_desc == NULL) {
    NaClLog(4, "SelLdrLauncher::StartModuleAndSetupAppChannel: "
            "Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the untrusted service
  // SRPC client takes an additional reference to untrusted_desc.
  if (!NaClSrpcClientCtor(out_app_chan, untrusted_desc->desc())) {
    NaClLog(4, "SelLdrLauncher::StartModuleAndSetupAppChannel: "
            "NaClSrpcClientCtor failed\n");
    return false;
  }
  return true;
}

DescWrapper* SelLdrLauncherBase::Wrap(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGeneric(raw_desc);
}

DescWrapper* SelLdrLauncherBase::WrapCleanup(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGenericCleanup(raw_desc);
}

nacl::string SelLdrLauncherBase::GetCrashLogOutput() {
  DescWrapper::MsgHeader hdr;
  DescWrapper::MsgIoVec iov;
  char msg_buf[1024];
  ssize_t nbytes = 0;

  iov.base = msg_buf;
  iov.length = sizeof msg_buf;
  hdr.iov = &iov;
  hdr.iov_length = 1;
  hdr.ndescv = NULL;
  hdr.ndescv_length = 0;
  hdr.flags = 0;
  if (NULL != bootstrap_socket_.get()) {
    nbytes = bootstrap_socket_->RecvMsg(&hdr, 0, NULL);
  }
  if (nbytes > 0) {
    return nacl::string(msg_buf, nbytes);
  }
  return "";
}

}  // namespace nacl
