// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_failures.h"

#include <map>

#include "base/logging.h"
#include "base/no_destructor.h"

namespace {

using FailureMap =
    std::map<extensions::ExtensionId,
             extensions::InstallationFailures::InstallationFailureData>;

FailureMap& GetInstallationFailureMap(const Profile* profile) {
  static base::NoDestructor<std::map<const Profile*, FailureMap>> failure_maps;
  return (*failure_maps)[profile];
}

}  // namespace

namespace extensions {

// static
void InstallationFailures::ReportFailure(const Profile* profile,
                                         const ExtensionId& id,
                                         Reason reason) {
  DCHECK_NE(reason, Reason::UNKNOWN);
  GetInstallationFailureMap(profile).emplace(
      id, std::make_pair(reason, base::nullopt));
}

// static
void InstallationFailures::ReportCrxInstallError(
    const Profile* profile,
    const ExtensionId& id,
    Reason reason,
    CrxInstallErrorDetail crx_install_error) {
  DCHECK(reason == Reason::CRX_INSTALL_ERROR_DECLINED ||
         reason == Reason::CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE ||
         reason == Reason::CRX_INSTALL_ERROR_OTHER);
  GetInstallationFailureMap(profile).emplace(
      id, std::make_pair(reason, crx_install_error));
}

// static
InstallationFailures::InstallationFailureData InstallationFailures::Get(
    const Profile* profile,
    const ExtensionId& id) {
  FailureMap& map = GetInstallationFailureMap(profile);
  auto it = map.find(id);
  return it == map.end() ? std::make_pair(Reason::UNKNOWN, base::nullopt)
                         : it->second;
}

// static
void InstallationFailures::Clear(const Profile* profile) {
  GetInstallationFailureMap(profile).clear();
}

}  //  namespace extensions
