/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "native_client/src/include/build_config.h"

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

SelLdrLauncherStandalone::SelLdrLauncherStandalone()
  : child_process_(NACL_INVALID_HANDLE),
    sel_ldr_locator_(new PluginSelLdrLocator()) {
}

void SelLdrLauncherStandalone::GetPluginDirectory(char* buffer, size_t len) {
  sel_ldr_locator_->GetDirectory(buffer, len);
}

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

void SelLdrLauncherStandalone::BuildCommandLine(vector<nacl::string>* command) {
  assert(sel_ldr_ != NACL_NO_FILE_PATH);  // Set by InitCommandLine().
  if (!command_prefix_.empty())
    command->insert(command->end(),
                    command_prefix_.begin(), command_prefix_.end());
  if (!sel_ldr_bootstrap_.empty())
    command->push_back(sel_ldr_bootstrap_);
  command->push_back(sel_ldr_);
  if (!sel_ldr_bootstrap_.empty()) {
    command->push_back("--r_debug=0xXXXXXXXXXXXXXXXX");
    command->push_back("--reserved_at_zero=0xXXXXXXXXXXXXXXXX");
  }
  command->push_back("-R");  // RPC will be used to point to the nexe.

  command->insert(command->end(), sel_ldr_argv_.begin(), sel_ldr_argv_.end());

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

  if (application_argv_.size() > 0) {
    // Separator between sel_universal and app args.
    command->push_back("--");
    command->insert(command->end(),
                    application_argv_.begin(), application_argv_.end());
  }
}

void SelLdrLauncherStandalone::InitCommandLine(
    const vector<nacl::string>& prefix,
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
  //      IRT to use.  The argument to "-X" is the inherited file
  //      descriptor over which the internal representation of the sock addr
  //      is sent in order to bootstrap communications with the untrusted
  //      NaCl module.
  sel_ldr_argv_.push_back("-X");
  sel_ldr_argv_.push_back(nacl::string(dest_fd));
}

void SelLdrLauncherStandalone::CloseHandlesAfterLaunch() {
  for (size_t i = 0; i < close_after_launch_.size(); i++) {
    NaClClose(close_after_launch_[i]);
  }
  close_after_launch_.clear();
}

bool SelLdrLauncherStandalone::Start(const char* url) {
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

}  // namespace nacl
