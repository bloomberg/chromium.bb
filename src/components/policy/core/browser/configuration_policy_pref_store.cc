// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_pref_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

void LogErrors(std::unique_ptr<PolicyErrorMap> errors,
               DeprecatedPoliciesSet deprecated_policies) {
  DCHECK(errors->IsReady());
  for (auto& pair : *errors) {
    base::string16 policy = base::ASCIIToUTF16(pair.first);
    DLOG(WARNING) << "Policy " << policy << ": " << pair.second;
  }
  for (const auto& policy : deprecated_policies) {
    DLOG(WARNING) << "Policy " << policy << ": "
                  << l10n_util::GetStringUTF16(IDS_POLICY_DEPRECATED);
  }
}

bool IsLevel(PolicyLevel level, const PolicyMap::const_iterator iter) {
  return iter->second.level == level;
}

}  // namespace

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    BrowserPolicyConnectorBase* policy_connector,
    PolicyService* service,
    const ConfigurationPolicyHandlerList* handler_list,
    PolicyLevel level)
    : policy_connector_(policy_connector),
      policy_service_(service),
      handler_list_(handler_list),
      level_(level) {
  // Read initial policy.
  prefs_.reset(CreatePreferencesFromPolicies());
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigurationPolicyPrefStore::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ConfigurationPolicyPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool ConfigurationPolicyPrefStore::IsInitializationComplete() const {
  return policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME);
}

bool ConfigurationPolicyPrefStore::GetValue(const std::string& key,
                                            const base::Value** value) const {
  const base::Value* stored_value = nullptr;
  if (!prefs_ || !prefs_->GetValue(key, &stored_value))
    return false;

  if (value)
    *value = stored_value;
  return true;
}

std::unique_ptr<base::DictionaryValue> ConfigurationPolicyPrefStore::GetValues()
    const {
  if (!prefs_)
    return std::make_unique<base::DictionaryValue>();
  return prefs_->AsDictionaryValue();
}

void ConfigurationPolicyPrefStore::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK_EQ(POLICY_DOMAIN_CHROME, ns.domain);
  DCHECK(ns.component_id.empty());
  Refresh();
}

void ConfigurationPolicyPrefStore::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME) {
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(true);
  }
}

ConfigurationPolicyPrefStore::~ConfigurationPolicyPrefStore() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::Refresh() {
  std::unique_ptr<PrefValueMap> new_prefs(CreatePreferencesFromPolicies());
  std::vector<std::string> changed_prefs;
  new_prefs->GetDifferingKeys(prefs_.get(), &changed_prefs);
  prefs_.swap(new_prefs);

  // Send out change notifications.
  for (std::vector<std::string>::const_iterator pref(changed_prefs.begin());
       pref != changed_prefs.end();
       ++pref) {
    for (auto& observer : observers_)
      observer.OnPrefValueChanged(*pref);
  }
}

PrefValueMap* ConfigurationPolicyPrefStore::CreatePreferencesFromPolicies() {
  std::unique_ptr<PrefValueMap> prefs(new PrefValueMap);
  PolicyMap filtered_policies;
  filtered_policies.CopyFrom(policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  filtered_policies.EraseNonmatching(base::BindRepeating(&IsLevel, level_));

  std::unique_ptr<PolicyErrorMap> errors = std::make_unique<PolicyErrorMap>();

  DeprecatedPoliciesSet deprecated_policies;
  handler_list_->ApplyPolicySettings(filtered_policies, prefs.get(),
                                     errors.get(), &deprecated_policies);

  if (!errors->empty()) {
    if (errors->IsReady()) {
      LogErrors(std::move(errors), std::move(deprecated_policies));
    } else if (policy_connector_) {  // May be null in tests.
      policy_connector_->NotifyWhenResourceBundleReady(base::BindOnce(
          &LogErrors, std::move(errors), std::move(deprecated_policies)));
    }
  }

  return prefs.release();
}

}  // namespace policy
