// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/status_provider/device_active_directory_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/ui/webui/policy/status_provider/status_provider_util.h"

DeviceActiveDirectoryPolicyStatusProvider::
    DeviceActiveDirectoryPolicyStatusProvider(
        policy::ActiveDirectoryPolicyManager* policy_manager,
        const std::string& enterprise_domain_manager)
    : UserActiveDirectoryPolicyStatusProvider(policy_manager, nullptr),
      enterprise_domain_manager_(enterprise_domain_manager) {}

void DeviceActiveDirectoryPolicyStatusProvider::GetStatus(
    base::DictionaryValue* dict) {
  UserActiveDirectoryPolicyStatusProvider::GetStatus(dict);
  dict->SetStringKey("enterpriseDomainManager", enterprise_domain_manager_);
}
