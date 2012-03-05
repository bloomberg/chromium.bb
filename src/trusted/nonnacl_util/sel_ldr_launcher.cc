/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

using std::vector;


namespace nacl {

SelLdrLauncher::SelLdrLauncher()
  : child_process_(kInvalidHandle),
    channel_(kInvalidHandle),
    bootstrap_socket_(NULL),
    socket_addr_(NULL),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

SelLdrLauncher::SelLdrLauncher(SelLdrLocator* sel_ldr_locator)
  : child_process_(kInvalidHandle),
    channel_(kInvalidHandle),
    bootstrap_socket_(NULL),
    socket_addr_(NULL),
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
  if (0 != desc->RecvMsg(&header, 0,
                         static_cast<struct NaClDescQuotaInterface*>(NULL))) {
    return NULL;
  }
  // Check that there was exactly one descriptor passed.
  if (1 != header.ndescv_length) {
    return NULL;
  }

  return descs[0];
}

bool SelLdrLauncher::SetupCommandAndLoad(NaClSrpcChannel* command,
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
    NaClLog(4, "SelLdrLauncher::SetupCommandAndLoad: "
            "getting sel_ldr socket address failed\n");
    return false;
  }
  // The first connection goes to the trusted command channel.
  scoped_ptr<DescWrapper> command_desc(socket_addr_->Connect());
  if (command_desc == NULL) {
    NaClLog(4, "SelLdrLauncher::SetupCommandAndLoad: Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes an additional reference to command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    NaClLog(4, "SelLdrLauncher::SetupCommandAndLoad: "
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
      NaClLog(4, "SelLdrLauncher::SetupCommandAndLoad: "
              "rpc_result= %d is not successful\n",
              static_cast<int>(rpc_result));
      NaClSrpcDtor(command);
      return false;
    }
  }
  return true;
}

bool
SelLdrLauncher::StartModuleAndSetupAppChannel(NaClSrpcChannel* command,
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

#ifdef NACL_STANDALONE
/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
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

void SelLdrLauncher::BuildCommandLine(vector<nacl::string>* command) {
  assert(sel_ldr_ != NACL_NO_FILE_PATH);  // Set by InitCommandLine().
  if (!command_prefix_.empty())
    command->insert(command->end(),
                    command_prefix_.begin(), command_prefix_.end());
  if (!sel_ldr_bootstrap_.empty())
    command->push_back(sel_ldr_bootstrap_);
  command->push_back(sel_ldr_);
  if (!sel_ldr_bootstrap_.empty())
    command->push_back("--r_debug=0xXXXXXXXXXXXXXXXX");
  command->push_back("-R");  // RPC will be used to point to the nexe.

  command->insert(command->end(), sel_ldr_argv_.begin(), sel_ldr_argv_.end());

  // Our use of "environ" above fails to link in the "shared" (DLL)
  // build of Chromium on Windows.  However, we do not use
  // BuildCommandLine() when integrated into Chromium anyway -- we use
  // sel_main_chrome.c -- so it is safe to disable this code, which is
  // largely for debugging.
  // TODO(mseaborn): Tidy this up so that we do not need a conditional.
#ifdef NACL_STANDALONE
  struct NaClEnvCleanser env_cleanser;
  NaClEnvCleanserCtor(&env_cleanser, 1);
  if (!NaClEnvCleanserInit(&env_cleanser, GetEnviron(), NULL)) {
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

void SelLdrLauncher::InitCommandLine(const vector<nacl::string>& prefix,
                                     const vector<nacl::string>& sel_ldr_argv,
                                     const vector<nacl::string>& app_argv) {
  assert(sel_ldr_ == NACL_NO_FILE_PATH);  // Make sure we don't call this twice.

  char* var = getenv("NACL_SEL_LDR");
  if (var != NULL) {
    sel_ldr_ = var;
  } else {
    sel_ldr_ = GetSelLdrPathName();
  }
  char *bootstrap_var = getenv("NACL_SEL_LDR_BOOTSTRAP");
  if (bootstrap_var != NULL) {
    sel_ldr_bootstrap_ = bootstrap_var;
  } else {
    sel_ldr_bootstrap_ = GetSelLdrBootstrapPathName();
  }
  // Add an emulator, if there is one.
  copy(prefix.begin(), prefix.end(), back_inserter(command_prefix_));
  command_prefix_ = prefix;
  // Copy the respective parameters.
  copy(sel_ldr_argv.begin(), sel_ldr_argv.end(), back_inserter(sel_ldr_argv_));
  copy(app_argv.begin(), app_argv.end(), back_inserter(application_argv_));
  nacl::string dest_fd;
  channel_ = CreateBootstrapSocket(&dest_fd);
  // Set up the bootstrap channel.
  // The arguments we want to pass to the service runtime are
  // "-X" causes the service runtime to create a bound socket and socket
  //      address at descriptors 3 and 4 in the untrusted code for the nexe or
  //      IRT to use.  The argument to "-X", dest_fd, is the file descriptor
  //      or Windows handle over which the internal representation of the sock
  //      addr is sent in order to bootstrap communications with the untrusted
  //      NaCl module.
  sel_ldr_argv_.push_back("-X");
  sel_ldr_argv_.push_back(dest_fd);
}

void SelLdrLauncher::CloseHandlesAfterLaunch() {
  for (size_t i = 0; i < close_after_launch_.size(); i++) {
    Close(close_after_launch_[i]);
  }
  close_after_launch_.clear();
}

DescWrapper* SelLdrLauncher::Wrap(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGeneric(raw_desc);
}

DescWrapper* SelLdrLauncher::WrapCleanup(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGenericCleanup(raw_desc);
}

#if defined(NACL_STANDALONE)
bool SelLdrLauncher::Start(const char* url) {
  UNREFERENCED_PARAMETER(url);
  vector<nacl::string> prefix;
  vector<nacl::string> args_for_sel_ldr;
  vector<nacl::string> args_for_nexe;
  const char* irt_library_path = getenv("NACL_IRT_LIBRARY");
  if (NULL != irt_library_path) {
    args_for_sel_ldr.push_back("-B");
    args_for_sel_ldr.push_back(irt_library_path);
  }
  if (!StartViaCommandLine(prefix, args_for_sel_ldr, args_for_nexe)) {
    return false;
  }
  return true;
}

bool SelLdrLauncher::Start(int socket_count,
                           Handle* result_sockets,
                           const char* url) {
  UNREFERENCED_PARAMETER(socket_count);
  UNREFERENCED_PARAMETER(result_sockets);
  return Start(url);
}
#endif  // defined(NACL_STANDALONE)

}  // namespace nacl
