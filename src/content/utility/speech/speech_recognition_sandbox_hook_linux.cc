// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/speech/speech_recognition_sandbox_hook_linux.h"

#include <dlfcn.h>

#include "components/soda/constants.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace speech {

namespace {

// Gets the file permissions required by the Speech On-Device API (SODA).
std::vector<BrokerFilePermission> GetSodaFilePermissions() {
  auto soda_dir = GetSodaDirectory();
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom")};

  // This may happen if a user doesn't have a SODA installation.
  if (!soda_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        soda_dir.AsEndingWithSeparator().value()));
  }

  // This may happen if a user doesn't have any language packs installed.
  auto language_packs_dir = GetSodaLanguagePacksDirectory();
  if (!language_packs_dir.empty()) {
    permissions.push_back(BrokerFilePermission::ReadOnlyRecursive(
        language_packs_dir.AsEndingWithSeparator().value()));
  }

  return permissions;
}

}  // namespace

bool SpeechRecognitionPreSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  void* soda_library = dlopen(GetSodaBinaryPath().value().c_str(),
                              RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  DCHECK(soda_library);

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_READLINK,
                                   sandbox::syscall_broker::COMMAND_STAT,
                               }),
                               GetSodaFilePermissions(),
                               sandbox::policy::SandboxLinux::PreSandboxHook(),
                               options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace speech
