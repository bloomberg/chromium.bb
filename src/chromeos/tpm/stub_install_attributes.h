// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_STUB_INSTALL_ATTRIBUTES_H_
#define CHROMEOS_TPM_STUB_INSTALL_ATTRIBUTES_H_

#include <string>

#include "chromeos/tpm/install_attributes.h"

namespace chromeos {

// This class allows tests to set specific configurations for testing.
class StubInstallAttributes : public InstallAttributes {
 public:
  // The default StubInstallAttributes has a DeviceMode of PENDING.
  StubInstallAttributes();

  StubInstallAttributes(const StubInstallAttributes&) = delete;
  StubInstallAttributes& operator=(const StubInstallAttributes&) = delete;

  // Creates a StubInstallAttributes and Clears it.
  static std::unique_ptr<StubInstallAttributes> CreateUnset();
  // Creates a StubInstallAttributes and calls SetConsumerOwned.
  static std::unique_ptr<StubInstallAttributes> CreateConsumerOwned();
  // Creates a StubInstallAttributes and calls SetCloudManaged.
  static std::unique_ptr<StubInstallAttributes> CreateCloudManaged(
      const std::string& domain,
      const std::string& device_id);
  // Creates a StubInstallAttributes and calls SetActiveDirectoryManaged.
  static std::unique_ptr<StubInstallAttributes> CreateActiveDirectoryManaged(
      const std::string& realm,
      const std::string& device_id);
  // Creates a StubInstallAttributes and calls SetDemoMode.
  static std::unique_ptr<StubInstallAttributes> CreateDemoMode(
      const std::string& device_id);

  // Setup as not-yet enrolled.
  void Clear();
  // Setup as consumer owned device. (Clears existing configuration.)
  void SetConsumerOwned();
  // Setup as managed by Google cloud. (Clears existing configuration.)
  void SetCloudManaged(const std::string& domain, const std::string& device_id);
  // Setup as managed by Active Directory server. (Clears existing
  // configuration.)
  void SetActiveDirectoryManaged(const std::string& realm,
                                 const std::string& device_id);

  // Setup as demo mode device with specified |device_id|. Clears existing
  // configuration.
  void SetDemoMode(const std::string& device_id);

  void set_device_locked(bool is_locked) { device_locked_ = is_locked; }
};

// Helper class to set install attributes in tests. Using one of the Create*
// methods injects the generated StubInstallAttributes into the singleton
// at chromeos::InstallAttributes::Get(). Scoping ensures that
// chromes::InstallAttributes::Shutdown is called when this is destructed.
class ScopedStubInstallAttributes {
 public:
  // Scopes the default StubInstallAttributes, with a DeviceMode of PENDING.
  ScopedStubInstallAttributes();
  // Scopes the given StubInstallAttributes.
  explicit ScopedStubInstallAttributes(
      std::unique_ptr<StubInstallAttributes> install_attributes);

  ScopedStubInstallAttributes(const ScopedStubInstallAttributes&) = delete;
  ScopedStubInstallAttributes& operator=(const ScopedStubInstallAttributes&) =
      delete;

  ~ScopedStubInstallAttributes();

  // Get the StubInstallAttributes that have been installed for modification.
  StubInstallAttributes* Get();

 private:
  // The InstallAttributes that are currently installed and that this
  // ScopedStubInstallAttributes is responsible for shutting down.
  std::unique_ptr<StubInstallAttributes> install_attributes_;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ScopedStubInstallAttributes;
using ::chromeos::StubInstallAttributes;
}  // namespace ash

#endif  // CHROMEOS_TPM_STUB_INSTALL_ATTRIBUTES_H_
