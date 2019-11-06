// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_

#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/file_system_provider/watcher.h"

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInfo;

// Fake implementation of the registry.
// For simplicity it can remember at most only one file system.
class FakeRegistry : public RegistryInterface {
 public:
  FakeRegistry();
  ~FakeRegistry() override;
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                          const Watchers& watchers) override;
  void ForgetFileSystem(const ProviderId& provider_id,
                        const std::string& file_system_id) override;
  std::unique_ptr<RestoredFileSystems> RestoreFileSystems(
      const ProviderId& provider_id) override;
  void UpdateWatcherTag(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher) override;
  const ProvidedFileSystemInfo* file_system_info() const;
  const Watchers* watchers() const;

 private:
  std::unique_ptr<ProvidedFileSystemInfo> file_system_info_;
  std::unique_ptr<Watchers> watchers_;

  DISALLOW_COPY_AND_ASSIGN(FakeRegistry);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_
