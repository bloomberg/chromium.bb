// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"

#include <string>
#include <utility>

namespace chrome_cleaner {

ForceInstalledExtension::ForceInstalledExtension(
    const ExtensionID& id,
    ExtensionInstallMethod install_method,
    const std::string& update_url,
    const std::string& manifest_permissions)
    : id(std::move(id)),
      install_method(std::move(install_method)),
      update_url(std::move(update_url)),
      manifest_permissions(std::move(manifest_permissions)) {}

ForceInstalledExtension& ForceInstalledExtension::operator=(
    const ForceInstalledExtension& other) = default;
ForceInstalledExtension& ForceInstalledExtension::operator=(
    ForceInstalledExtension&& other) = default;
ForceInstalledExtension::ForceInstalledExtension(
    ExtensionID id,
    ExtensionInstallMethod install_method)
    : id(std::move(id)), install_method(std::move(install_method)) {}

ForceInstalledExtension::ForceInstalledExtension(
    const ForceInstalledExtension& extension) = default;
ForceInstalledExtension::ForceInstalledExtension(
    ForceInstalledExtension&& extension) = default;

ForceInstalledExtension::~ForceInstalledExtension() = default;

bool ForceInstalledExtension::operator==(
    const ForceInstalledExtension& other) const {
  // Don't include policy pointers in comparison because that metadata
  // is only used when writing out the results of transforming the values.
  return id == other.id && install_method == other.install_method &&
         update_url == other.update_url &&
         manifest_permissions == other.manifest_permissions;
}

bool ForceInstalledExtension::operator<(
    const ForceInstalledExtension& other) const {
  if (id < other.id) {
    return true;
  } else if (id > other.id) {
    return false;
  } else if (install_method < other.install_method) {
    return true;
  } else if (install_method > other.install_method) {
    return false;
  } else if (update_url < other.update_url) {
    return true;
  } else if (update_url > other.update_url) {
    return false;
  } else if (manifest_permissions < other.manifest_permissions) {
    return true;
  } else if (manifest_permissions > other.manifest_permissions) {
    return false;
  }
  return false;
}

}  // namespace chrome_cleaner
