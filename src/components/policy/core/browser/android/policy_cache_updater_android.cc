// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/policy_cache_updater_android.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "components/policy/android/jni_headers/PolicyCacheUpdater_jni.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/android/policy_map_android.h"
#include "components/policy/core/common/policy_namespace.h"

namespace policy {
namespace android {

namespace {

bool CanCache(PolicyErrorMap* policy_error_map,
              const std::set<std::string>& future_policies,
              const PolicyMap::const_iterator iter) {
  return !policy_error_map->HasError(iter->first) &&
         future_policies.find(iter->first) == future_policies.end() &&
         !iter->second.ignored() &&
         !iter->second.HasMessage(PolicyMap::MessageType::kError);
}

}  // namespace

PolicyCacheUpdater::PolicyCacheUpdater(
    PolicyService* policy_service,
    const ConfigurationPolicyHandlerList* handler_list)
    : policy_service_(policy_service), handler_list_(handler_list) {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  UpdateCache(
      policy_service->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, "")));
}

PolicyCacheUpdater::~PolicyCacheUpdater() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void PolicyCacheUpdater::OnPolicyUpdated(const PolicyNamespace& ns,
                                         const PolicyMap& previous,
                                         const PolicyMap& current) {
  UpdateCache(current);
}

void PolicyCacheUpdater::UpdateCache(const PolicyMap& current_policy_map) {
  PolicyMap policy_map = current_policy_map.Clone();
  PolicyErrorMap errors;
  std::set<std::string> future_policies;
  handler_list_->ApplyPolicySettings(policy_map,
                                     /*prefs=*/nullptr, &errors,
                                     /*deprecated_policies*/ nullptr,
                                     &future_policies);
  policy_map.EraseNonmatching(
      base::BindRepeating(&CanCache, &errors, future_policies));
  PolicyMapAndroid policy_map_android(policy_map);
  Java_PolicyCacheUpdater_cachePolicies(base::android::AttachCurrentThread(),
                                        policy_map_android.GetJavaObject());
}

}  // namespace android
}  // namespace policy
