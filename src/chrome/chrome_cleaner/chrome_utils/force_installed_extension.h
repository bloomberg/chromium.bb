// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CHROME_UTILS_FORCE_INSTALLED_EXTENSION_H_
#define CHROME_CHROME_CLEANER_CHROME_UTILS_FORCE_INSTALLED_EXTENSION_H_

#include <memory>
#include <string>

#include "chrome/chrome_cleaner/chrome_utils/extension_id.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"

namespace chrome_cleaner {

// Forward declare to avoid dependency cycle.
struct ExtensionPolicyRegistryEntry;
struct ExtensionPolicyFile;

// An extension that has been force installed.
struct ForceInstalledExtension {
  ForceInstalledExtension(const ExtensionID& id,
                          ExtensionInstallMethod install_method,
                          const std::string& update_url,
                          const std::string& manifest_permissions);
  ForceInstalledExtension(ExtensionID id,
                          ExtensionInstallMethod install_method);

  ForceInstalledExtension(const ForceInstalledExtension& extension);
  ForceInstalledExtension(ForceInstalledExtension&& extension);
  ForceInstalledExtension& operator=(const ForceInstalledExtension& other);
  ForceInstalledExtension& operator=(ForceInstalledExtension&& other);
  ~ForceInstalledExtension();
  bool operator==(const ForceInstalledExtension& other) const;
  bool operator<(const ForceInstalledExtension& other) const;

  ExtensionID id;
  ExtensionInstallMethod install_method;
  std::string update_url;
  std::string manifest_permissions;
  std::shared_ptr<ExtensionPolicyRegistryEntry> policy_registry_entry;
  std::shared_ptr<ExtensionPolicyFile> policy_file;
};

// Overload to compare by Extension IDs, useful when constructing sets
struct ExtensionIDCompare {
  bool operator()(const ForceInstalledExtension& lhs,
                  const ForceInstalledExtension& rhs) const {
    return lhs.id < rhs.id;
  }
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CHROME_UTILS_FORCE_INSTALLED_EXTENSION_H_
