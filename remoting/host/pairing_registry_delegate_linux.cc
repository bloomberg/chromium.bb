// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/host/branding.h"

namespace {
const char kRegistryFilename[] = "paired-clients.json";
}  // namespace

namespace remoting {

using protocol::PairingRegistry;

PairingRegistryDelegateLinux::PairingRegistryDelegateLinux(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(task_runner),
      weak_factory_(this) {
}

PairingRegistryDelegateLinux::~PairingRegistryDelegateLinux() {
}

void PairingRegistryDelegateLinux::Save(
    const std::string& pairings_json,
    const PairingRegistry::SaveCallback& callback) {
  // If a callback was supplied, wrap it in a helper function that will
  // run it on this thread.
  PairingRegistry::SaveCallback run_callback_on_this_thread;
  if (!callback.is_null()) {
    run_callback_on_this_thread =
        base::Bind(&PairingRegistryDelegateLinux::RunSaveCallbackOnThread,
                   base::ThreadTaskRunnerHandle::Get(),
                   callback);
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PairingRegistryDelegateLinux::DoSave,
                 weak_factory_.GetWeakPtr(),
                 pairings_json,
                 run_callback_on_this_thread));
}

void PairingRegistryDelegateLinux::Load(
    const PairingRegistry::LoadCallback& callback) {
  // Wrap the callback in a helper function that will run it on this thread.
  // Note that, unlike AddPairing, the GetPairing callback is mandatory.
  PairingRegistry::LoadCallback run_callback_on_this_thread =
      base::Bind(&PairingRegistryDelegateLinux::RunLoadCallbackOnThread,
                 base::ThreadTaskRunnerHandle::Get(),
                 callback);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PairingRegistryDelegateLinux::DoLoad,
                 weak_factory_.GetWeakPtr(),
                 run_callback_on_this_thread));
}

void PairingRegistryDelegateLinux::RunSaveCallbackOnThread(
    scoped_refptr<base::TaskRunner> task_runner,
    const PairingRegistry::SaveCallback& callback,
    bool success) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, success));
}

void PairingRegistryDelegateLinux::RunLoadCallbackOnThread(
      scoped_refptr<base::TaskRunner> task_runner,
      const PairingRegistry::LoadCallback& callback,
      const std::string& pairings_json) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, pairings_json));
}

void PairingRegistryDelegateLinux::DoSave(
    const std::string& pairings_json,
    const PairingRegistry::SaveCallback& callback) {
  base::FilePath registry_path = GetRegistryFilePath();
  base::FilePath parent_directory = registry_path.DirName();
  base::PlatformFileError error;
  if (!file_util::CreateDirectoryAndGetError(parent_directory, &error)) {
    LOG(ERROR) << "Could not create pairing registry directory: " << error;
    return;
  }
  if (!base::ImportantFileWriter::WriteFileAtomically(registry_path,
                                                      pairings_json)) {
    LOG(ERROR) << "Could not save pairing registry.";
  }

  if (!callback.is_null()) {
    callback.Run(true);
  }
}

void PairingRegistryDelegateLinux::DoLoad(
    const PairingRegistry::LoadCallback& callback) {
  base::FilePath registry_path = GetRegistryFilePath();
  std::string result;
  if (!file_util::ReadFileToString(registry_path, &result)) {
    LOG(ERROR) << "Load failed.";
  }
  callback.Run(result);
}

base::FilePath PairingRegistryDelegateLinux::GetRegistryFilePath() {
  if (!filename_for_testing_.empty()) {
    return filename_for_testing_;
  }

  base::FilePath config_dir = remoting::GetConfigDir();
  return config_dir.Append(kRegistryFilename);
}

void PairingRegistryDelegateLinux::SetFilenameForTesting(
    const base::FilePath &filename) {
  filename_for_testing_ = filename;
}


scoped_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate(
    scoped_refptr<base::TaskRunner> task_runner) {
  return scoped_ptr<PairingRegistry::Delegate>(
      new PairingRegistryDelegateLinux(task_runner));
}

}  // namespace remoting
