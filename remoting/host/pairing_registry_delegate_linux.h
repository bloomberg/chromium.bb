// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_
#define REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_

#include "remoting/protocol/pairing_registry.h"

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace base {
class ListValue;
class TaskRunner;
}  // namespace base

namespace remoting {

class PairingRegistryDelegateLinux
    : public protocol::PairingRegistry::Delegate {
 public:
  explicit PairingRegistryDelegateLinux(
      scoped_refptr<base::TaskRunner> task_runner);
  virtual ~PairingRegistryDelegateLinux();

  // PairingRegistry::Delegate interface
  virtual void Save(
      const std::string& pairings_json,
      const protocol::PairingRegistry::SaveCallback& callback) OVERRIDE;
  virtual void Load(
      const protocol::PairingRegistry::LoadCallback& callback) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, SaveAndLoad);

  // Blocking helper methods run using the TaskRunner passed to the ctor.
  void DoSave(const std::string& pairings_json,
              const protocol::PairingRegistry::SaveCallback& callback);
  void DoLoad(const protocol::PairingRegistry::LoadCallback& callback);

  // Run the delegate callbacks on their original thread.
  static void RunSaveCallbackOnThread(
      scoped_refptr<base::TaskRunner> task_runner,
      const protocol::PairingRegistry::SaveCallback& callback,
      bool success);
  static void RunLoadCallbackOnThread(
      scoped_refptr<base::TaskRunner> task_runner,
      const protocol::PairingRegistry::LoadCallback& callback,
      const std::string& pairings_json);

  // Helper methods to load and save the pairing registry.
  protocol::PairingRegistry::PairedClients LoadPairings();
  void SavePairings(
      const protocol::PairingRegistry::PairedClients& paired_clients);

  // Return the path to the file to use for loading and saving paired clients.
  base::FilePath GetRegistryFilePath();

  // For testing purposes, set the path returned by |GetRegistryFilePath|.
  void SetFilenameForTesting(const base::FilePath &filename);

  scoped_refptr<base::TaskRunner> task_runner_;
  base::FilePath filename_for_testing_;

  base::WeakPtrFactory<PairingRegistryDelegateLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistryDelegateLinux);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_
