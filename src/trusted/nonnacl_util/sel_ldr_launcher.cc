/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

using std::string;
using std::vector;

// TODO(robertm): Move this to  new header if it becomes more popular
template <class T> std::string ToString(const T& t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}


namespace nacl {

SelLdrLauncher::SelLdrLauncher()
  : child_(kInvalidHandle),
    channel_(kInvalidHandle),
    sock_addr_(NULL),
    is_sel_ldr_(true),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

SelLdrLauncher::SelLdrLauncher(SelLdrLocator* sel_ldr_locator)
  : child_(kInvalidHandle),
    channel_(kInvalidHandle),
    sock_addr_(NULL),
    is_sel_ldr_(true),
    sel_ldr_locator_(sel_ldr_locator) {
}

void SelLdrLauncher::GetPluginDirectory(char* buffer, size_t len) {
  sel_ldr_locator_->GetDirectory(buffer, len);
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


string SelLdrLauncher::ExpandVar(string arg) {
  if (arg[0] != '$') return arg;

  if (arg == "$SEL_LDR_EXE") {
    return sel_ldr_;
  } else if (arg == "$APP_EXE") {
    return application_name_;
  } else if (arg == "$CHAN_BUF") {
    return channel_buf_;
  } else if (arg == "$CHAN") {
    return channel_number_;
  } else {
    // TODO(robertm): add error message
    return "BAD_BAD_BAD";
  }
}


void SelLdrLauncher::BuildArgv(vector<string>* command) {
  // assert that both Init() and SetChannelBuf() were called
  assert(channel_buf_ != "");
  assert(sel_ldr_ != "");
  // NOTE: this is followed by sel_ldr_argv, "--", application_argv
  static const char* kCommandLineTemplate[] = {
    "$SEL_LDR_EXE",        // something like sel_ldr
    "-f", "$APP_EXE",      // something like app_name.nexe
    "-i", "$CHAN_BUF",     // something like <NaCl fd>:<imcchannel#>
    0,
  };

  for (size_t i = 0; kCommandLineTemplate[i]; ++i) {
    command->push_back(ExpandVar(kCommandLineTemplate[i]));
  }

  for (size_t i = 0; i < sel_ldr_argv_.size(); ++i) {
    command->push_back(ExpandVar(sel_ldr_argv_[i]));
  }

  if (application_argv_.size() > 0) {
    // Separator between sel_universal and app args
    command->push_back("--");

    for (size_t i = 0; i < application_argv_.size(); ++i) {
      command->push_back(ExpandVar(application_argv_[i]));
    }
  }
}


void SelLdrLauncher::Init(const string& application_name,
                          int imc_fd,
                          const vector<string>& sel_ldr_argv,
                          const vector<string>& app_argv) {
  // make sure we don't call this twice
  assert(sel_ldr_ == "");
  sel_ldr_ = GetSelLdrPathName();
  application_name_ = application_name;
  copy(sel_ldr_argv.begin(), sel_ldr_argv.end(), back_inserter(sel_ldr_argv_));
  copy(app_argv.begin(), app_argv.end(), back_inserter(application_argv_));
  channel_number_ = ToString(imc_fd);
}

//
void SelLdrLauncher::InitChannelBuf(Handle imc_channel_handle) {
  channel_buf_ = channel_number_ + ":" +
                 // TODO(robertm): eliminate #ifdef
                 // NOTE: before we used printf("%d:%u", ...)
#if NACL_WINDOWS
                 ToString(reinterpret_cast<uintptr_t>(imc_channel_handle));
#else
                 ToString(imc_channel_handle);
#endif
}

}  // namespace nacl
