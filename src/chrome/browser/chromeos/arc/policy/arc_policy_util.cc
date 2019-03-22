// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"

namespace arc {
namespace policy_util {

namespace {

// Constants used to parse ARC++ JSON policy.
constexpr char kApplicationsKey[] = "applications";
constexpr char kInstallTypeKey[] = "installType";
constexpr char kPackageNameKey[] = "packageName";
constexpr char kInstallTypeRequired[] = "REQUIRED";
constexpr char kInstallTypeForceInstalled[] = "FORCE_INSTALLED";

}  // namespace

bool IsAccountManaged(const Profile* profile) {
  return policy::ProfilePolicyConnectorFactory::IsProfileManaged(profile);
}

bool IsArcDisabledForEnterprise() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnterpriseDisableArc);
}

EcryptfsMigrationAction GetDefaultEcryptfsMigrationActionForManagedUser(
    bool active_directory_user) {
  // Active directory users are assumed to be enterprise users, so mimic the
  // server-side default logic for enterprise users, letting them choose if
  // they want to migrate by default.
  return active_directory_user ? EcryptfsMigrationAction::kAskUser
                               : EcryptfsMigrationAction::kDisallowMigration;
}

std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy) {
  std::unique_ptr<base::Value> dict = base::JSONReader::Read(arc_policy);
  if (!dict || !dict->is_dict())
    return {};

  const base::Value* const packages =
      dict->FindKeyOfType(kApplicationsKey, base::Value::Type::LIST);
  if (!packages)
    return {};

  std::set<std::string> requested_packages;
  for (const auto& package : packages->GetList()) {
    if (!package.is_dict())
      continue;
    const base::Value* const install_type =
        package.FindKeyOfType(kInstallTypeKey, base::Value::Type::STRING);
    if (!install_type)
      continue;
    if (install_type->GetString() != kInstallTypeRequired &&
        install_type->GetString() != kInstallTypeForceInstalled) {
      continue;
    }
    const base::Value* const package_name =
        package.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);
    if (!package_name || package_name->GetString().empty())
      continue;
    requested_packages.insert(package_name->GetString());
  }
  return requested_packages;
}

}  // namespace policy_util
}  // namespace arc
