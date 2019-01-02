// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/extension_policy_migrator.h"

namespace policy {

ExtensionPolicyMigrator::~ExtensionPolicyMigrator() {}

void ExtensionPolicyMigrator::CopyPoliciesIfUnset(
    PolicyBundle* bundle,
    const std::string& extension_id,
    base::span<const Migration> migrations) {
  PolicyMap& extension_map = bundle->Get(
      PolicyNamespace(PolicyDomain::POLICY_DOMAIN_EXTENSIONS, extension_id));
  if (extension_map.empty())
    return;

  PolicyMap& chrome_map = bundle->Get(PolicyNamespace(
      PolicyDomain::POLICY_DOMAIN_CHROME, /* component_id */ std::string()));

  for (const auto& migration : migrations) {
    PolicyMap::Entry* entry = extension_map.GetMutable(migration.old_name);
    if (entry) {
      if (!chrome_map.Get(migration.new_name)) {
        chrome_map.Set(migration.new_name, entry->DeepCopy());
      }
      // TODO(crbug/869958): Mark the old policy as deprecated for
      // chrome://policy.
    }
  }
}

}  // namespace policy
