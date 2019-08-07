// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_sandbox_hook.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace chromeos {
namespace ime {

namespace {

// This is IME decoder shared library, which will be built from google3.
const char kLibImeDecoderName[] = "libimedecoder.so";
// This is input method's relative folder, where the pre-installed dictionaries
// will be put.
const char kInputToolsBundleFolder[] = "input_methods/input_tools";
// This is where shared dictionaries will be put, decoder will load them for
// all users and regularly update them from server.
const char kSharedHomePath[] = "/home/chronos/ime/";
// This is where user's dictionaries will be put, decoder will load them for
// current user and save new words learnt from user.
const char kUserHomePath[] = "/home/chronos/user/ime/";

bool CreateFolderIfNotExist(const char* dir) {
  base::FilePath path = base::FilePath(dir);
  return base::CreateDirectory(path);
}

// This is where IME decoder shared library will be put.
base::FilePath GetLibFolder() {
#if defined(__x86_64__) || defined(__aarch64__)
  return base::FilePath("/usr/lib64");
#else
  return base::FilePath("/usr/lib");
#endif
}

// This is where input method's application bundle will be put.
base::FilePath GetChromeOSAssetFolder() {
  return base::FilePath("/usr/share/chromeos-assets");
}

void AddDecoderPath(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath lib_path = GetLibFolder().AppendASCII(kLibImeDecoderName);
  permissions->push_back(BrokerFilePermission::ReadOnly(lib_path.value()));
}

void AddBundleFolder(std::vector<BrokerFilePermission>* permissions) {
  // This is the IME application bundle folder, where pre-installed dictionaries
  // are put, which decoder needs to read.
  base::FilePath bundle_dir = GetChromeOSAssetFolder()
                                  .AppendASCII(kInputToolsBundleFolder)
                                  .AsEndingWithSeparator();
  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive(bundle_dir.value()));
}

void AddSharedDataFolder(std::vector<BrokerFilePermission>* permissions) {
  // Must have access to shared home folder, otherwise decoder won't be able
  // to work.
  CHECK(CreateFolderIfNotExist(kSharedHomePath));
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive(kSharedHomePath));
}

void AddUserDataFolder(std::vector<BrokerFilePermission>* permissions) {
  // When failed to access user profile folder, decoder still can work, but
  // user dictionary can not be saved.
  bool success = CreateFolderIfNotExist(kUserHomePath);
  if (!success) {
    LOG(WARNING) << "Unable to create ime folder under user profile folder";
  }
  // Still need to push this path, otherwise process will crash directly when
  // decoder tries to access this folder.
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive(kUserHomePath));
}

std::vector<BrokerFilePermission> GetImeFilePermissions() {
  // These 2 paths are needed before creating IME service.
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu")};
  AddDecoderPath(&permissions);
  AddBundleFolder(&permissions);
  AddSharedDataFolder(&permissions);
  AddUserDataFolder(&permissions);
  return permissions;
}

}  // namespace

bool ImePreSandboxHook(service_manager::SandboxLinux::Options options) {
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_MKDIR,
                                   sandbox::syscall_broker::COMMAND_STAT,
                                   sandbox::syscall_broker::COMMAND_RENAME,
                                   sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetImeFilePermissions(),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);

  instance->EngageNamespaceSandboxIfPossible();
  return true;
}

}  // namespace ime
}  // namespace chromeos
