// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_
#define REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_

#include "remoting/protocol/pairing_registry.h"

#include "base/files/file_path.h"

namespace base {
class ListValue;
}  // namespace base

namespace remoting {

class PairingRegistryDelegateLinux
    : public protocol::PairingRegistry::Delegate {
 public:
  PairingRegistryDelegateLinux();
  virtual ~PairingRegistryDelegateLinux();

  // PairingRegistry::Delegate interface
  virtual scoped_ptr<base::ListValue> LoadAll() OVERRIDE;
  virtual bool DeleteAll() OVERRIDE;
  virtual protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) OVERRIDE;
  virtual bool Save(const protocol::PairingRegistry::Pairing& pairing) OVERRIDE;
  virtual bool Delete(const std::string& client_id) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, SaveAndLoad);
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, Stateless);

  // Return the path to the directory to use for loading and saving paired
  // clients.
  base::FilePath GetRegistryPath();

  // For testing purposes, set the path returned by |GetRegistryPath()|.
  void SetRegistryPathForTesting(const base::FilePath& registry_path);

  base::FilePath registry_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistryDelegateLinux);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_DELEGATE_LINUX_H_
