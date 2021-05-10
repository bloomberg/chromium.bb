// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "components/policy/core/common/android/policy_service_android.h"
#endif

namespace policy {

namespace {

const char* kProxyPolicies[] = {
  key::kProxyMode,
  key::kProxyServerMode,
  key::kProxyServer,
  key::kProxyPacUrl,
  key::kProxyBypassList,
};

// Maps the separate policies for proxy settings into a single Dictionary
// policy. This allows to keep the logic of merging policies from different
// sources simple, as all separate proxy policies should be considered as a
// single whole during merging.
void RemapProxyPolicies(PolicyMap* policies) {
  // The highest (level, scope) pair for an existing proxy policy is determined
  // first, and then only policies with those exact attributes are merged.
  PolicyMap::Entry current_priority;  // Defaults to the lowest priority.
  PolicySource inherited_source = POLICY_SOURCE_ENTERPRISE_DEFAULT;
  base::Value proxy_settings(base::Value::Type::DICTIONARY);
  for (size_t i = 0; i < base::size(kProxyPolicies); ++i) {
    const PolicyMap::Entry* entry = policies->Get(kProxyPolicies[i]);
    if (entry) {
      if (entry->has_higher_priority_than(current_priority)) {
        proxy_settings = base::Value(base::Value::Type::DICTIONARY);
        current_priority = entry->DeepCopy();
        if (entry->source > inherited_source)  // Higher priority?
          inherited_source = entry->source;
      }
      if (!entry->has_higher_priority_than(current_priority) &&
          !current_priority.has_higher_priority_than(*entry)) {
        proxy_settings.SetKey(kProxyPolicies[i], entry->value()->Clone());
      }
      policies->Erase(kProxyPolicies[i]);
    }
  }
  // Sets the new |proxy_settings| if kProxySettings isn't set yet, or if the
  // new priority is higher.
  const PolicyMap::Entry* existing = policies->Get(key::kProxySettings);
  if (!proxy_settings.DictEmpty() &&
      (!existing || current_priority.has_higher_priority_than(*existing))) {
    policies->Set(key::kProxySettings, current_priority.level,
                  current_priority.scope, inherited_source,
                  std::move(proxy_settings), nullptr);
  }
}

// Maps the separate renamed policies into a single Dictionary policy. This
// allows to keep the logic of merging policies from different sources simple,
// as all separate renamed policies should be considered as a single whole
// during merging.
void RemapRenamedPolicies(PolicyMap* policies) {
  // For all renamed policies we need to explicitly merge the value of the
  // old policy with the new one or else merging will not be carried over
  // if desired.
  base::Value* merge_list =
      policies->GetMutableValue(key::kPolicyListMultipleSourceMergeList);
  base::flat_set<std::string> policy_lists_to_merge =
      policy::ValueToStringSet(merge_list);
  const std::vector<std::pair<const char*, const char*>> renamed_policies = {{
      {policy::key::kSafeBrowsingWhitelistDomains,
       policy::key::kSafeBrowsingAllowlistDomains},
      {policy::key::kSpellcheckLanguageBlacklist,
       policy::key::kSpellcheckLanguageBlocklist},
      {policy::key::kURLBlacklist, policy::key::kURLBlocklist},
      {policy::key::kURLWhitelist, policy::key::kURLAllowlist},
#if !defined(OS_ANDROID)
      {policy::key::kAutoplayWhitelist, policy::key::kAutoplayAllowlist},
#endif  // !defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_EXTENSIONS)
      {policy::key::kExtensionInstallBlacklist,
       policy::key::kExtensionInstallBlocklist},
      {policy::key::kExtensionInstallWhitelist,
       policy::key::kExtensionInstallAllowlist},
      {policy::key::kNativeMessagingBlacklist,
       policy::key::kNativeMessagingBlocklist},
      {policy::key::kNativeMessagingWhitelist,
       policy::key::kNativeMessagingAllowlist},
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
      {policy::key::kAttestationExtensionWhitelist,
       policy::key::kAttestationExtensionAllowlist},
      {policy::key::kExternalPrintServersWhitelist,
       policy::key::kExternalPrintServersAllowlist},
      {policy::key::kNativePrintersBulkBlacklist,
       policy::key::kPrintersBulkBlocklist},
      {policy::key::kNativePrintersBulkWhitelist,
       policy::key::kPrintersBulkAllowlist},
      {policy::key::kPerAppTimeLimitsWhitelist,
       policy::key::kPerAppTimeLimitsAllowlist},
      {policy::key::kQuickUnlockModeWhitelist,
       policy::key::kQuickUnlockModeAllowlist},
      {policy::key::kNoteTakingAppsLockScreenWhitelist,
       policy::key::kNoteTakingAppsLockScreenAllowlist},
#if defined(USE_CUPS)
      {policy::key::kPrintingAPIExtensionsWhitelist,
       policy::key::kPrintingAPIExtensionsAllowlist},
#endif  // defined(USE_CUPS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }};
  for (const auto& policy_pair : renamed_policies) {
    PolicyMap::Entry* old_policy = policies->GetMutable(policy_pair.first);
    const PolicyMap::Entry* new_policy = policies->Get(policy_pair.second);
    if (old_policy &&
        (!new_policy || old_policy->has_higher_priority_than(*new_policy))) {
      PolicyMap::Entry policy_entry = old_policy->DeepCopy();
      policy_entry.AddMessage(PolicyMap::MessageType::kWarning,
                              IDS_POLICY_MIGRATED_NEW_POLICY,
                              {base::UTF8ToUTF16(policy_pair.first)});
      old_policy->AddMessage(PolicyMap::MessageType::kError,
                             IDS_POLICY_MIGRATED_OLD_POLICY,
                             {base::UTF8ToUTF16(policy_pair.second)});
      policies->Set(policy_pair.second, std::move(policy_entry));
    }
    if (policy_lists_to_merge.contains(policy_pair.first) &&
        !policy_lists_to_merge.contains(policy_pair.second)) {
      merge_list->Append(base::Value(policy_pair.second));
    }
  }
}

// Metrics should not be enforced so if this policy is set as mandatory
// downgrade it to a recommended level policy.
void DowngradeMetricsReportingToRecommendedPolicy(PolicyMap* policies) {
  PolicyMap::Entry* policy =
      policies->GetMutable(policy::key::kMetricsReportingEnabled);
  if (policy && policy->level != POLICY_LEVEL_RECOMMENDED && policy->value() &&
      policy->value()->is_bool() && policy->value()->GetBool()) {
    policy->level = POLICY_LEVEL_RECOMMENDED;
    policy->AddMessage(PolicyMap::MessageType::kInfo,
                       IDS_POLICY_IGNORED_MANDATORY_REPORTING_POLICY);
  }
}

// Returns the string values of |policy|. Returns an empty set if the values are
// not strings.
base::flat_set<std::string> GetStringListPolicyItems(
    const PolicyBundle& bundle,
    const PolicyNamespace& space,
    const std::string& policy) {
  return ValueToStringSet(bundle.Get(space).GetValue(policy));
}

}  // namespace

PolicyServiceImpl::PolicyServiceImpl(Providers providers, Migrators migrators)
    : PolicyServiceImpl(std::move(providers),
                        std::move(migrators),
                        /*initialization_throttled=*/false) {}

PolicyServiceImpl::PolicyServiceImpl(Providers providers,
                                     Migrators migrators,
                                     bool initialization_throttled)
    : providers_(std::move(providers)),
      migrators_(std::move(migrators)),
      initialization_throttled_(initialization_throttled) {
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    policy_domain_status_[domain] = PolicyDomainStatus::kUninitialized;

  for (auto* provider : providers_)
    provider->AddObserver(this);
  CheckPolicyDomainStatus();
  // There are no observers yet, but calls to GetPolicies() should already get
  // the processed policy values.
  MergeAndTriggerUpdates();
}

// static
std::unique_ptr<PolicyServiceImpl>
PolicyServiceImpl::CreateWithThrottledInitialization(Providers providers,
                                                     Migrators migrators) {
  return base::WrapUnique(
      new PolicyServiceImpl(std::move(providers), std::move(migrators),
                            /*initialization_throttled=*/true));
}

PolicyServiceImpl::~PolicyServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto* provider : providers_)
    provider->RemoveObserver(this);
}

void PolicyServiceImpl::AddObserver(PolicyDomain domain,
                                    PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_[domain].AddObserver(observer);
}

void PolicyServiceImpl::RemoveObserver(PolicyDomain domain,
                                       PolicyService::Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = observers_.find(domain);
  if (it == observers_.end())
    return;
  it->second.RemoveObserver(observer);
  if (it->second.empty()) {
    observers_.erase(it);
  }
}

void PolicyServiceImpl::AddProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_update_observers_.AddObserver(observer);
}

void PolicyServiceImpl::RemoveProviderUpdateObserver(
    ProviderUpdateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  provider_update_observers_.RemoveObserver(observer);
}

bool PolicyServiceImpl::HasProvider(
    ConfigurationPolicyProvider* provider) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Contains(providers_, provider);
}

const PolicyMap& PolicyServiceImpl::GetPolicies(
    const PolicyNamespace& ns) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return policy_bundle_.Get(ns);
}

bool PolicyServiceImpl::IsInitializationComplete(PolicyDomain domain) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return !initialization_throttled_ &&
         policy_domain_status_[domain] != PolicyDomainStatus::kUninitialized;
}

bool PolicyServiceImpl::IsFirstPolicyLoadComplete(PolicyDomain domain) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(domain >= 0 && domain < POLICY_DOMAIN_SIZE);
  return !initialization_throttled_ &&
         policy_domain_status_[domain] == PolicyDomainStatus::kPolicyReady;
}

void PolicyServiceImpl::RefreshPolicies(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!callback.is_null())
    refresh_callbacks_.push_back(std::move(callback));

  if (providers_.empty()) {
    // Refresh is immediately complete if there are no providers. See the note
    // on OnUpdatePolicy() about why this is a posted task.
    update_task_ptr_factory_.InvalidateWeakPtrs();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                  update_task_ptr_factory_.GetWeakPtr()));
  } else {
    // Some providers might invoke OnUpdatePolicy synchronously while handling
    // RefreshPolicies. Mark all as pending before refreshing.
    for (auto* provider : providers_)
      refresh_pending_.insert(provider);
    for (auto* provider : providers_)
      provider->RefreshPolicies();
  }
}

#if defined(OS_ANDROID)
android::PolicyServiceAndroid* PolicyServiceImpl::GetPolicyServiceAndroid() {
  if (!policy_service_android_)
    policy_service_android_ =
        std::make_unique<android::PolicyServiceAndroid>(this);
  return policy_service_android_.get();
}
#endif

void PolicyServiceImpl::UnthrottleInitialization() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialization_throttled_)
    return;

  initialization_throttled_ = false;
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain)
    MaybeNotifyPolicyDomainStatusChange(static_cast<PolicyDomain>(domain));
}

void PolicyServiceImpl::OnUpdatePolicy(ConfigurationPolicyProvider* provider) {
  DCHECK_EQ(1, std::count(providers_.begin(), providers_.end(), provider));
  refresh_pending_.erase(provider);
  provider_update_pending_.insert(provider);

  // Note: a policy change may trigger further policy changes in some providers.
  // For example, disabling SigninAllowed would cause the CloudPolicyManager to
  // drop all its policies, which makes this method enter again for that
  // provider.
  //
  // Therefore this update is posted asynchronously, to prevent reentrancy in
  // MergeAndTriggerUpdates. Also, cancel a pending update if there is any,
  // since both will produce the same PolicyBundle.
  update_task_ptr_factory_.InvalidateWeakPtrs();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PolicyServiceImpl::MergeAndTriggerUpdates,
                                update_task_ptr_factory_.GetWeakPtr()));
}

void PolicyServiceImpl::NotifyNamespaceUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iterator = observers_.find(ns.domain);
  if (iterator != observers_.end()) {
    for (auto& observer : iterator->second)
      observer.OnPolicyUpdated(ns, previous, current);
  }
}

void PolicyServiceImpl::NotifyProviderUpdatesPropagated() {
  if (provider_update_pending_.empty())
    return;

  for (auto& provider_update_observer : provider_update_observers_) {
    for (ConfigurationPolicyProvider* provider : provider_update_pending_) {
      provider_update_observer.OnProviderUpdatePropagated(provider);
    }
  }
  provider_update_pending_.clear();
}

void PolicyServiceImpl::MergeAndTriggerUpdates() {
  // Merge from each provider in their order of priority.
  const PolicyNamespace chrome_namespace(POLICY_DOMAIN_CHROME, std::string());
  PolicyBundle bundle;
  for (auto* provider : providers_) {
    PolicyBundle provided_bundle;
    provided_bundle.CopyFrom(provider->policies());
    RemapProxyPolicies(&provided_bundle.Get(chrome_namespace));
    RemapRenamedPolicies(&provided_bundle.Get(chrome_namespace));
    DowngradeMetricsReportingToRecommendedPolicy(
        &provided_bundle.Get(chrome_namespace));
    bundle.MergeFrom(provided_bundle);
  }

  // Merges all the mergeable policies
  base::flat_set<std::string> policy_lists_to_merge = GetStringListPolicyItems(
      bundle, chrome_namespace, key::kPolicyListMultipleSourceMergeList);
  base::flat_set<std::string> policy_dictionaries_to_merge =
      GetStringListPolicyItems(bundle, chrome_namespace,
                               key::kPolicyDictionaryMultipleSourceMergeList);

  auto& chrome_policies = bundle.Get(chrome_namespace);

  // This has to be done after setting enterprise default values since it is
  // enabled by default for enterprise users.
  auto* atomic_policy_group_enabled_policy_value =
      chrome_policies.Get(key::kPolicyAtomicGroupsEnabled);

  // This policy has to be ignored if it comes from a user signed-in profile.
  bool atomic_policy_group_enabled =
      atomic_policy_group_enabled_policy_value &&
      atomic_policy_group_enabled_policy_value->value()->GetBool() &&
      !((atomic_policy_group_enabled_policy_value->source ==
             POLICY_SOURCE_CLOUD ||
         atomic_policy_group_enabled_policy_value->source ==
             POLICY_SOURCE_PRIORITY_CLOUD) &&
        atomic_policy_group_enabled_policy_value->scope == POLICY_SCOPE_USER);

  PolicyListMerger policy_list_merger(std::move(policy_lists_to_merge));
  PolicyDictionaryMerger policy_dictionary_merger(
      std::move(policy_dictionaries_to_merge));

  std::vector<PolicyMerger*> mergers{&policy_list_merger,
                                     &policy_dictionary_merger};

  PolicyGroupMerger policy_group_merger;
  if (atomic_policy_group_enabled)
    mergers.push_back(&policy_group_merger);

  for (auto it = bundle.begin(); it != bundle.end(); ++it)
    it->second->MergeValues(mergers);

  for (auto& migrator : migrators_)
    migrator->Migrate(&bundle);

  // Swap first, so that observers that call GetPolicies() see the current
  // values.
  policy_bundle_.Swap(&bundle);

  // Only notify observers of namespaces that have been modified.
  const PolicyMap kEmpty;
  PolicyBundle::const_iterator it_new = policy_bundle_.begin();
  PolicyBundle::const_iterator end_new = policy_bundle_.end();
  PolicyBundle::const_iterator it_old = bundle.begin();
  PolicyBundle::const_iterator end_old = bundle.end();
  while (it_new != end_new && it_old != end_old) {
    if (it_new->first < it_old->first) {
      // A new namespace is available.
      NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);
      ++it_new;
    } else if (it_old->first < it_new->first) {
      // A previously available namespace is now gone.
      NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);
      ++it_old;
    } else {
      if (!it_new->second->Equals(*it_old->second)) {
        // An existing namespace's policies have changed.
        NotifyNamespaceUpdated(it_new->first, *it_old->second, *it_new->second);
      }
      ++it_new;
      ++it_old;
    }
  }

  // Send updates for the remaining new namespaces, if any.
  for (; it_new != end_new; ++it_new)
    NotifyNamespaceUpdated(it_new->first, kEmpty, *it_new->second);

  // Sends updates for the remaining removed namespaces, if any.
  for (; it_old != end_old; ++it_old)
    NotifyNamespaceUpdated(it_old->first, *it_old->second, kEmpty);

  CheckPolicyDomainStatus();
  CheckRefreshComplete();
  NotifyProviderUpdatesPropagated();
}

void PolicyServiceImpl::CheckPolicyDomainStatus() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Check if all the providers just became initialized for each domain; if so,
  // notify that domain's observers. If they were initialized, check if they had
  // their first policies loaded.
  for (int domain = 0; domain < POLICY_DOMAIN_SIZE; ++domain) {
    PolicyDomain policy_domain = static_cast<PolicyDomain>(domain);
    if (policy_domain_status_[domain] == PolicyDomainStatus::kPolicyReady)
      continue;

    PolicyDomainStatus new_status = PolicyDomainStatus::kPolicyReady;

    for (auto* provider : providers_) {
      if (!provider->IsInitializationComplete(policy_domain)) {
        new_status = PolicyDomainStatus::kUninitialized;
        break;
      } else if (!provider->IsFirstPolicyLoadComplete(policy_domain)) {
        new_status = PolicyDomainStatus::kInitialized;
      }
    }

    if (new_status == policy_domain_status_[domain])
      continue;

    policy_domain_status_[domain] = new_status;
    MaybeNotifyPolicyDomainStatusChange(policy_domain);
  }
}
void PolicyServiceImpl::MaybeNotifyPolicyDomainStatusChange(
    PolicyDomain policy_domain) {
  if (initialization_throttled_ || policy_domain_status_[policy_domain] ==
                                       PolicyDomainStatus::kUninitialized) {
    return;
  }

  auto iter = observers_.find(policy_domain);
  if (iter == observers_.end())
    return;

  for (auto& observer : iter->second) {
    observer.OnPolicyServiceInitialized(policy_domain);
    if (policy_domain_status_[policy_domain] ==
        PolicyDomainStatus::kPolicyReady)
      observer.OnFirstPoliciesLoaded(policy_domain);
  }
}

void PolicyServiceImpl::CheckRefreshComplete() {
  // Invoke all the callbacks if a refresh has just fully completed.
  if (refresh_pending_.empty() && !refresh_callbacks_.empty()) {
    std::vector<base::OnceClosure> callbacks;
    callbacks.swap(refresh_callbacks_);
    for (auto& callback : callbacks)
      std::move(callback).Run();
  }
}

}  // namespace policy
