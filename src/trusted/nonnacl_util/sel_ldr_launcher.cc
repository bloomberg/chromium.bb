/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>

#include <algorithm>
#include <iterator>
#include <vector>

#if NACL_OSX
#include <crt_externs.h>
#endif

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"

using std::vector;


namespace nacl {

SelLdrLauncher::SelLdrLauncher()
  : child_process_(kInvalidHandle),
    channel_(kInvalidHandle),
    command_prefix_(""),
    socket_address_(NULL),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

SelLdrLauncher::SelLdrLauncher(SelLdrLocator* sel_ldr_locator)
  : child_process_(kInvalidHandle),
    channel_(kInvalidHandle),
    socket_address_(NULL),
    sel_ldr_locator_(sel_ldr_locator) {
  CHECK(sel_ldr_locator != NULL);
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
                                      NaClSrpcChannel* untrusted) {
  DescWrapperFactory factory;
  socket_address_.reset(factory.MakeImcSock(channel_));
  if (socket_address_ == NULL) {
    return false;
  }

  // channel_desc now has ownership of channel_, so we get rid of our
  // "reference" to it.
  channel_ = kInvalidHandle;

  // Get the socket address from the descriptor.
  scoped_ptr<DescWrapper> sock_addr(GetSockAddr(socket_address_.get()));
  if (sock_addr == NULL) {
    NaClLog(4, "SelLdrLauncher::OpenSrpcChannels: GetSockAddr failed\n");
    return false;
  }

  // The first connection goes to the trusted command channel.
  scoped_ptr<DescWrapper> command_desc(sock_addr->Connect());
  if (command_desc == NULL) {
    NaClLog(4, "SelLdrLauncher::OpenSrpcChannels: Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes ownership of command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    return false;
  }

  // Start untrusted code module
  int start_result;
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(command, "start_module::i", &start_result)) {
    return false;
  }

  // The second connection goes to the service itself.
  scoped_ptr<DescWrapper> untrusted_desc(sock_addr->Connect());
  if (untrusted_desc ==  NULL) {
    return false;
  }
  // Start the SRPC client to communicate with the untrusted service
  // SRPC client takes ownership of untrusted_desc.
  if (!NaClSrpcClientCtor(untrusted, untrusted_desc->desc())) {
    return false;
  }

  return true;
}


#ifdef NACL_STANDALONE
extern "C" char **environ;

static char **GetEnviron() {
#if NACL_OSX
  /* Mac dynamic libraries cannot access the environ variable directly. */
  return *_NSGetEnviron();
#else
  return environ;
#endif
}
#endif

void SelLdrLauncher::SetCommandPrefix(const nacl::string& prefix) {
  command_prefix_ = prefix;
}

void SelLdrLauncher::BuildCommandLine(vector<nacl::string>* command) {
  assert(sel_ldr_ != NACL_NO_FILE_PATH);  // Set by InitCommandLine().
  if (command_prefix_.size() > 0) {
    command->push_back(command_prefix_);
  }
  command->push_back(sel_ldr_);
  if (application_file_ != NACL_NO_FILE_PATH) {
    command->push_back("-f");
    command->push_back(application_file_);
  } else {
    command->push_back("-R");  // RPC will be used to point to the nexe.
  }

  command->insert(command->end(), sel_ldr_argv_.begin(), sel_ldr_argv_.end());

  // Our use of "environ" above fails to link in the "shared" (DLL)
  // build of Chromium on Windows.  However, we do not use
  // BuildCommandLine() when integrated into Chromium anyway -- we use
  // sel_main_chrome.c -- so it is safe to disable this code, which is
  // largely for debugging.
  // TODO(mseaborn): Tidy this up so that we do not need a conditional.
#ifdef NACL_STANDALONE
  struct NaClEnvCleanser env_cleanser;
  NaClEnvCleanserCtor(&env_cleanser);
  if (!NaClEnvCleanserInit(&env_cleanser, GetEnviron())) {
    NaClLog(LOG_FATAL, "Failed to initialise env cleanser\n");
  }
  for (const char* const* env = NaClEnvCleanserEnvironment(&env_cleanser);
       *env != NULL;
       ++env) {
    command->push_back("-E");
    command->push_back(*env);
  }
  NaClEnvCleanserDtor(&env_cleanser);
#endif

  if (application_argv_.size() > 0) {
    // Separator between sel_universal and app args.
    command->push_back("--");
    command->insert(command->end(),
                    application_argv_.begin(), application_argv_.end());
  }
}

void SelLdrLauncher::InitCommandLine(const nacl::string& app_file,
                                     int imc_fd,
                                     const vector<nacl::string>& sel_ldr_argv,
                                     const vector<nacl::string>& app_argv) {
  assert(sel_ldr_ == NACL_NO_FILE_PATH);  // Make sure we don't call this twice.

  char* var = getenv("NACL_SEL_LDR");
  if (var != NULL) {
    sel_ldr_ = var;
  } else {
    sel_ldr_ = GetSelLdrPathName();
  }
  application_file_ = app_file;
  copy(sel_ldr_argv.begin(), sel_ldr_argv.end(), back_inserter(sel_ldr_argv_));
  copy(app_argv.begin(), app_argv.end(), back_inserter(application_argv_));
  channel_number_ = imc_fd;
}

void SelLdrLauncher::CloseHandlesAfterLaunch() {
  for (size_t i = 0; i < close_after_launch_.size(); i++) {
    Close(close_after_launch_[i]);
  }
  close_after_launch_.clear();
}

}  // namespace nacl
