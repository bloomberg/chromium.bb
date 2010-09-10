/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>

#include <algorithm>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using std::vector;


namespace nacl {

SelLdrLauncher::SelLdrLauncher()
  : child_(kInvalidHandle),
    channel_(kInvalidHandle),
    sock_addr_(NULL),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

SelLdrLauncher::SelLdrLauncher(SelLdrLocator* sel_ldr_locator)
  : child_(kInvalidHandle),
    channel_(kInvalidHandle),
    sock_addr_(NULL),
    sel_ldr_locator_(sel_ldr_locator) {
}

void SelLdrLauncher::GetPluginDirectory(char* buffer, size_t len) {
  sel_ldr_locator_->GetDirectory(buffer, len);
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
      NaClSrpcInvokeBySignature(command, "start_module::i", &start_result)) {
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
  delete command_desc;
  delete untrusted_command_desc;
  delete untrusted_desc;
  delete channel_desc;
  delete sock_addr;

  return retval;
}


void SelLdrLauncher::BuildArgv(vector<nacl::string>* command) {
  // assert that Init() was called
  assert(sel_ldr_ != "");

  command->push_back(sel_ldr_);
  command->push_back("-f");
  command->push_back(application_name_);

  for (size_t i = 0; i < sel_ldr_argv_.size(); ++i) {
    command->push_back(sel_ldr_argv_[i]);
  }

  if (application_argv_.size() > 0) {
    // Separator between sel_universal and app args
    command->push_back("--");

    for (size_t i = 0; i < application_argv_.size(); ++i) {
      command->push_back(application_argv_[i]);
    }
  }
}


void SelLdrLauncher::Init(const nacl::string& application_name,
                          int imc_fd,
                          const vector<nacl::string>& sel_ldr_argv,
                          const vector<nacl::string>& app_argv) {
  // make sure we don't call this twice
  assert(sel_ldr_ == "");
  char* var = getenv("NACL_SEL_LDR");
  if (var != NULL) {
    sel_ldr_ = var;
  } else {
    sel_ldr_ = GetSelLdrPathName();
  }
  application_name_ = application_name;
  copy(sel_ldr_argv.begin(), sel_ldr_argv.end(), back_inserter(sel_ldr_argv_));
  copy(app_argv.begin(), app_argv.end(), back_inserter(application_argv_));
  channel_number_ = imc_fd;
}

void SelLdrLauncher::CloseHandlesAfterLaunch() {
  for(size_t i = 0; i < close_after_launch_.size(); i++) {
    Close(close_after_launch_[i]);
  }
  close_after_launch_.clear();
}

}  // namespace nacl
