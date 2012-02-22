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
    channel_number_(-1),
    command_prefix_(""),
    bootstrap_socket_(NULL),
    socket_addr_(NULL),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

SelLdrLauncher::SelLdrLauncher(SelLdrLocator* sel_ldr_locator)
  : child_process_(kInvalidHandle),
    channel_(kInvalidHandle),
    channel_number_(-1),
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

bool SelLdrLauncher::LoadModule(NaClSrpcChannel* command, DescWrapper* nexe) {
  NaClLog(4, "Entered SelLdrLauncher::LoadModule\n");
  NaClSrpcResultCodes rpc_result = NaClSrpcInvokeBySignature(command,
                                                             "load_module:hs:",
                                                             nexe->desc(),
                                                             "place holder");
  NaClLog(4, "SelLdrLauncher::LoadModule rpc result %d\n", (int) rpc_result);
  return NACL_SRPC_RESULT_OK == rpc_result;
}

bool SelLdrLauncher::StartModule(NaClSrpcChannel* command,
                                 NaClErrorCode* code) {
  // Start untrusted code module
  NaClLog(4, "Entered SelLdrLauncher::StartModule\n");
  int start_result;
  NaClSrpcResultCodes rpc_result = NaClSrpcInvokeBySignature(command,
                                                             "start_module::i",
                                                             &start_result);
  NaClLog(4, "SelLdrLauncher::StartModule rpc result %d\n", (int) rpc_result);
  if (NACL_SRPC_RESULT_OK != rpc_result) {
    NaClSrpcDtor(command);
    *code = LOAD_INTERNAL;
    NaClLog(4, "Leaving SelLdrLauncher::StartModule, FAILED\n");
    return false;
  }

  *code = (NaClErrorCode) start_result;
  NaClLog(4, "Leaving SelLdrLauncher::StartModule, success\n");
  return true;
}


bool SelLdrLauncher::SetupBootstrapChannel() {
  // channel_ is initialized in LaunchFromCommandLine if
  // channel_number_ is not -1, and InitCommandLine requires imc_fd to be
  // supplied which is used to initialize channel_number_, so channel_ should
  // never be invalid.

  CHECK(factory_ == NULL);
  factory_.reset(new DescWrapperFactory);

  CHECK(channel_ != kInvalidHandle);
  bootstrap_socket_.reset(factory_->MakeImcSock(channel_));
  if (bootstrap_socket_ == NULL) {
    NaClLog(4, ("Leaving SelLdrLauncher::SetupBootstrapChannel"
                " SetupBootstrapChannel failed\n"));
    return false;
  }

  // bootstrap_socket_ now has ownership of channel_, so we get rid of
  // our "reference" to it.
  channel_ = kInvalidHandle;
  return true;
}

bool SelLdrLauncher::GetLdrSocketAddress() {
  // Get the socket address from the descriptor.
  socket_addr_.reset(GetSockAddr(bootstrap_socket_.get()));
  if (socket_addr_ == NULL) {
    NaClLog(4, "SelLdrLauncher::GetLdrSocketAddress: GetSockAddr failed\n");
    return false;
  }
  return true;
}

bool SelLdrLauncher::SetupCommandChannel(NaClSrpcChannel* command) {
  // The first connection goes to the trusted command channel.
  scoped_ptr<DescWrapper> command_desc(socket_addr_->Connect());
  if (command_desc == NULL) {
    NaClLog(4, "SelLdrLauncher::SetupCommandChannel: Connect failed\n");
    return false;
  }
  // Start the SRPC client to communicate with the trusted command channel.
  // SRPC client takes an additional reference to command_desc.
  if (!NaClSrpcClientCtor(command, command_desc->desc())) {
    NaClLog(4, "SelLdrLauncher::SetupCommandChannel: command ctor failed\n");
    return false;
  }
  return true;
}

bool SelLdrLauncher::SetupApplicationChannel(NaClSrpcChannel* app_channel) {
  // The second connection goes to the service itself.
  scoped_ptr<DescWrapper> untrusted_desc(socket_addr_->Connect());
  if (untrusted_desc == NULL) {
    return false;
  }
  // Start the SRPC client to communicate with the untrusted service
  // SRPC client takes an additional reference to untrusted_desc.
  if (!NaClSrpcClientCtor(app_channel, untrusted_desc->desc())) {
    NaClLog(4,
            "SelLdrLauncher::SetupApplicationChannel: untrusted ctor failed\n");
    return false;
  }
  return true;
}

bool SelLdrLauncher::SetupCommandAndLoad(NaClSrpcChannel* command,
                                         DescWrapper* nexe) {
  if (!SetupBootstrapChannel()) {
    return false;
  }
  if (!GetLdrSocketAddress()) {
    return false;
  }
  if (!SetupCommandChannel(command)) {
    return false;
  }
  if (NULL != nexe) {
    if (!LoadModule(command, nexe)) {
      NaClSrpcDtor(command);
      return false;
    }
  }
  return true;
}

bool
SelLdrLauncher::StartModuleAndSetupAppChannel(NaClSrpcChannel* command,
                                              NaClSrpcChannel* out_app_chan) {
  NaClErrorCode code;
  if (!StartModule(command, &code)) {
    NaClLog(4,
            ("SelLdrLauncher::StartModuleAndSetupAppChannel: start module"
             " failed %d (%s) \n"),
            (int) code,
            NaClErrorString(code));
    return false;
  }
  if (!SetupApplicationChannel(out_app_chan)) {
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

void SelLdrLauncher::SetCommandPrefix(const nacl::string& prefix) {
  command_prefix_ = prefix;
}

void SelLdrLauncher::BuildCommandLine(vector<nacl::string>* command) {
  assert(sel_ldr_ != NACL_NO_FILE_PATH);  // Set by InitCommandLine().
  if (!command_prefix_.empty())
    command->push_back(command_prefix_);
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

void SelLdrLauncher::InitCommandLine(int imc_fd,
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

DescWrapper* SelLdrLauncher::Wrap(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGeneric(raw_desc);
}

DescWrapper* SelLdrLauncher::WrapCleanup(NaClDesc* raw_desc) {
  CHECK(factory_ != NULL);
  return factory_->MakeGenericCleanup(raw_desc);
}

#if defined(NACL_STANDALONE)
bool SelLdrLauncher::Start(int socket_count,
                           Handle* result_sockets,
                           const char* url) {
  UNREFERENCED_PARAMETER(url);
  // Temporary: this interface is highly tailored to use by the browser
  // plugin, which currently wants to have exactly three sockets.
  // 0 -- the "bootstrap" socket.
  // 1 -- the "async receive" socket.
  // 2 -- the "async send" socket.
  // TODO(sehr): Change this API to return only the bootstrap socket when
  // the async support is removed.
  if (socket_count != 3) {
    return false;
  }
  // The arguments we want to pass to the service runtime are
  // "-X 5" causes the service runtime to create a bound socket and socket
  //      address at descriptors 3 and 4 in the untrusted code for the nexe
  //      or IRT to use.  The argument to "-X", 5, is the host-OS descriptor
  //      over which the internal representation of the sock addr is sent in
  //      order to bootstrap communications with the untrusted NaCl module.
  //      (NB: 5 is in a completely different namespace from the 3,4.)
  const char* kSelLdrArgs[] = { "-X", "5" };
  const int kSelLdrArgLength = NACL_ARRAY_SIZE(kSelLdrArgs);
  vector<nacl::string> args_for_sel_ldr(kSelLdrArgs,
                                        kSelLdrArgs + kSelLdrArgLength);
  vector<nacl::string> args_for_nexe;
  const char* irt_library_path = getenv("NACL_IRT_LIBRARY");
  if (NULL != irt_library_path) {
    args_for_sel_ldr.push_back("-B");
    args_for_sel_ldr.push_back(irt_library_path);
  }
  // This "5" matches the "-X 5" argument above.
  InitCommandLine(5, args_for_sel_ldr, args_for_nexe);

  result_sockets[1] = ExportImcFD(6);
  if (result_sockets[1] == nacl::kInvalidHandle) {
    return false;
  }
  result_sockets[2] = ExportImcFD(7);
  if (result_sockets[2] == nacl::kInvalidHandle) {
    return false;
  }
  if (!LaunchFromCommandLine()) {
    return false;
  }
  result_sockets[0] = channel_;
  return true;
}
#endif  // defined(NACL_STANDALONE)

}  // namespace nacl
