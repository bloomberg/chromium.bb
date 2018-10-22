// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_sandbox_hook_linux.h"

#include <dlfcn.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "sandbox/linux/syscall_broker/broker_command.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace audio {

namespace {

#if defined(USE_ALSA)
void AddAlsaFilePermissions(std::vector<BrokerFilePermission>* permissions) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  const base::FilePath kAsoundrc =
      home_dir.Append(FILE_PATH_LITERAL(".asoundrc"));
  const std::string kReadOnlyFilenames[]{"/etc/asound.conf", "/proc/cpuinfo",
                                         "/etc/group", "/etc/nsswitch.conf",
                                         kAsoundrc.value()};
  for (const auto& filename : kReadOnlyFilenames)
    permissions->push_back(BrokerFilePermission::ReadOnly(filename));

  permissions->push_back(
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/alsa/"));
  permissions->push_back(
      BrokerFilePermission::ReadWriteCreateRecursive("/dev/snd/"));

  static const base::FilePath::CharType kDevAloadPath[] =
      FILE_PATH_LITERAL("/dev/aloadC");
  for (int i = 0; i <= 31; ++i) {
    permissions->push_back(BrokerFilePermission::ReadWrite(
        base::StringPrintf("%s%d", kDevAloadPath, i)));
  }
}
#endif

#if defined(USE_PULSEAUDIO)
void AddPulseAudioFilePermissions(
    std::vector<BrokerFilePermission>* permissions) {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  const base::FilePath kXauthorityPath =
      home_dir.Append(FILE_PATH_LITERAL(".Xauthority"));

  // Calling read() system call on /proc/self/exe returns broker process' path,
  // and it's used by pulse audio for creating a new context.
  const std::string kReadOnlyFilenames[]{
      "/etc/machine-id", "/proc/self/exe",
      "/usr/lib/x86_64-linux-gnu/gconv/gconv-modules.cache",
      "/usr/lib/x86_64-linux-gnu/gconv/gconv-modules", kXauthorityPath.value()};
  for (const auto& filename : kReadOnlyFilenames)
    permissions->push_back(BrokerFilePermission::ReadOnly(filename));

  const base::FilePath kPulsePath =
      home_dir.Append(FILE_PATH_LITERAL(".pulse/"));
  const base::FilePath kConfigPulsePath =
      home_dir.Append(FILE_PATH_LITERAL(".config/pulse/"));
  const std::string kReadOnlyRecursivePaths[]{"/etc/pulse/", kPulsePath.value(),
                                              kConfigPulsePath.value()};
  for (const auto& path : kReadOnlyRecursivePaths)
    permissions->push_back(BrokerFilePermission::ReadOnlyRecursive(path));
}
#endif

std::vector<BrokerFilePermission> GetAudioFilePermissions() {
  std::vector<BrokerFilePermission> permissions{
      BrokerFilePermission::ReadOnly("/dev/urandom"),
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu"),
      BrokerFilePermission::ReadOnlyRecursive("/usr/share/locale/"),
      BrokerFilePermission::ReadWriteCreateRecursive("/dev/shm/")};

#if defined(USE_PULSEAUDIO)
  AddPulseAudioFilePermissions(&permissions);
#endif
#if defined(USE_ALSA)
  AddAlsaFilePermissions(&permissions);
#endif

  return permissions;
}

void LoadAudioLibraries() {
  const std::string kLibraries[]{"libasound.so.2", "libpulse.so.0",
                                 "libpulsecommon-11.1.so", "libnss_files.so.2"};
  for (const auto& library_name : kLibraries) {
    if (nullptr ==
        dlopen(library_name.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE)) {
      LOG(WARNING) << "dlopen: failed to open " << library_name
                   << " with error: " << dlerror();
    }
  }
}

}  // namespace

bool AudioPreSandboxHook(service_manager::SandboxLinux::Options options) {
  LoadAudioLibraries();
  auto* instance = service_manager::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_ACCESS,
                                   sandbox::syscall_broker::COMMAND_OPEN,
                                   sandbox::syscall_broker::COMMAND_READLINK,
                                   sandbox::syscall_broker::COMMAND_STAT,
                                   sandbox::syscall_broker::COMMAND_UNLINK,
                               }),
                               GetAudioFilePermissions(),
                               service_manager::SandboxLinux::PreSandboxHook(),
                               options);

  // TODO(https://crbug.com/850878) enable namespace sandbox. Currently, if
  // enabled, connect() on pulse native socket fails with ENOENT (called from
  // pa_context_connect).

  return true;
}

}  // namespace audio
