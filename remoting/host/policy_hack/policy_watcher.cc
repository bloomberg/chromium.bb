// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_hack/policy_watcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/host/dns_blackhole_checker.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

namespace remoting {
namespace policy_hack {

namespace {

// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kFallbackReloadDelayMinutes = 15;

// Copies all policy values from one dictionary to another, using values from
// |default| if they are not set in |from|, or values from |bad_type_values| if
// the value in |from| has the wrong type.
scoped_ptr<base::DictionaryValue> CopyGoodValuesAndAddDefaults(
    const base::DictionaryValue* from,
    const base::DictionaryValue* default_values,
    const base::DictionaryValue* bad_type_values) {
  scoped_ptr<base::DictionaryValue> to(default_values->DeepCopy());
  for (base::DictionaryValue::Iterator i(*default_values);
       !i.IsAtEnd(); i.Advance()) {

    const base::Value* value = NULL;

    // If the policy isn't in |from|, use the default.
    if (!from->Get(i.key(), &value)) {
      continue;
    }

    // If the policy is the wrong type, use the value from |bad_type_values|.
    if (!value->IsType(i.value().GetType())) {
      CHECK(bad_type_values->Get(i.key(), &value));
    }

    to->Set(i.key(), value->DeepCopy());
  }

#if !defined(NDEBUG)
  // Replace values with those specified in DebugOverridePolicies, if present.
  std::string policy_overrides;
  if (from->GetString(PolicyWatcher::kHostDebugOverridePoliciesName,
                      &policy_overrides)) {
    scoped_ptr<base::Value> value(base::JSONReader::Read(policy_overrides));
    const base::DictionaryValue* override_values;
    if (value && value->GetAsDictionary(&override_values)) {
      to->MergeDictionary(override_values);
    }
  }
#endif  // defined(NDEBUG)

  return to.Pass();
}

}  // namespace

const char PolicyWatcher::kNatPolicyName[] =
    "RemoteAccessHostFirewallTraversal";

const char PolicyWatcher::kHostRequireTwoFactorPolicyName[] =
    "RemoteAccessHostRequireTwoFactor";

const char PolicyWatcher::kHostDomainPolicyName[] =
    "RemoteAccessHostDomain";

const char PolicyWatcher::kHostMatchUsernamePolicyName[] =
    "RemoteAccessHostMatchUsername";

const char PolicyWatcher::kHostTalkGadgetPrefixPolicyName[] =
    "RemoteAccessHostTalkGadgetPrefix";

const char PolicyWatcher::kHostRequireCurtainPolicyName[] =
    "RemoteAccessHostRequireCurtain";

const char PolicyWatcher::kHostTokenUrlPolicyName[] =
    "RemoteAccessHostTokenUrl";

const char PolicyWatcher::kHostTokenValidationUrlPolicyName[] =
    "RemoteAccessHostTokenValidationUrl";

const char PolicyWatcher::kHostTokenValidationCertIssuerPolicyName[] =
    "RemoteAccessHostTokenValidationCertificateIssuer";

const char PolicyWatcher::kHostAllowClientPairing[] =
    "RemoteAccessHostAllowClientPairing";

const char PolicyWatcher::kHostAllowGnubbyAuthPolicyName[] =
    "RemoteAccessHostAllowGnubbyAuth";

const char PolicyWatcher::kRelayPolicyName[] =
    "RemoteAccessHostAllowRelayedConnection";

const char PolicyWatcher::kUdpPortRangePolicyName[] =
    "RemoteAccessHostUdpPortRange";

const char PolicyWatcher::kHostDebugOverridePoliciesName[] =
    "RemoteAccessHostDebugOverridePolicies";

PolicyWatcher::PolicyWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      old_policies_(new base::DictionaryValue()),
      default_values_(new base::DictionaryValue()),
      weak_factory_(this) {
  // Initialize the default values for each policy.
  default_values_->SetBoolean(kNatPolicyName, true);
  default_values_->SetBoolean(kHostRequireTwoFactorPolicyName, false);
  default_values_->SetBoolean(kHostRequireCurtainPolicyName, false);
  default_values_->SetBoolean(kHostMatchUsernamePolicyName, false);
  default_values_->SetString(kHostDomainPolicyName, std::string());
  default_values_->SetString(kHostTalkGadgetPrefixPolicyName,
                               kDefaultHostTalkGadgetPrefix);
  default_values_->SetString(kHostTokenUrlPolicyName, std::string());
  default_values_->SetString(kHostTokenValidationUrlPolicyName, std::string());
  default_values_->SetString(kHostTokenValidationCertIssuerPolicyName,
                             std::string());
  default_values_->SetBoolean(kHostAllowClientPairing, true);
  default_values_->SetBoolean(kHostAllowGnubbyAuthPolicyName, true);
  default_values_->SetBoolean(kRelayPolicyName, true);
  default_values_->SetString(kUdpPortRangePolicyName, "");
#if !defined(NDEBUG)
  default_values_->SetString(kHostDebugOverridePoliciesName, std::string());
#endif

  // Initialize the fall-back values to use for unreadable policies.
  // For most policies these match the defaults.
  bad_type_values_.reset(default_values_->DeepCopy());
  bad_type_values_->SetBoolean(kNatPolicyName, false);
  bad_type_values_->SetBoolean(kRelayPolicyName, false);
}

PolicyWatcher::~PolicyWatcher() {
}

void PolicyWatcher::StartWatching(const PolicyCallback& policy_callback) {
  if (!OnPolicyWatcherThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&PolicyWatcher::StartWatching,
                                      base::Unretained(this),
                                      policy_callback));
    return;
  }

  policy_callback_ = policy_callback;
  StartWatchingInternal();
}

void PolicyWatcher::StopWatching(base::WaitableEvent* done) {
  if (!OnPolicyWatcherThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&PolicyWatcher::StopWatching,
                                      base::Unretained(this), done));
    return;
  }

  StopWatchingInternal();
  weak_factory_.InvalidateWeakPtrs();
  policy_callback_.Reset();

  done->Signal();
}

void PolicyWatcher::ScheduleFallbackReloadTask() {
  DCHECK(OnPolicyWatcherThread());
  ScheduleReloadTask(
      base::TimeDelta::FromMinutes(kFallbackReloadDelayMinutes));
}

void PolicyWatcher::ScheduleReloadTask(const base::TimeDelta& delay) {
  DCHECK(OnPolicyWatcherThread());
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PolicyWatcher::Reload, weak_factory_.GetWeakPtr()),
      delay);
}

const base::DictionaryValue& PolicyWatcher::Defaults() const {
  return *default_values_;
}

bool PolicyWatcher::OnPolicyWatcherThread() const {
  return task_runner_->BelongsToCurrentThread();
}

void PolicyWatcher::UpdatePolicies(
    const base::DictionaryValue* new_policies_raw) {
  DCHECK(OnPolicyWatcherThread());

  // Use default values for any missing policies.
  scoped_ptr<base::DictionaryValue> new_policies =
      CopyGoodValuesAndAddDefaults(
          new_policies_raw, default_values_.get(), bad_type_values_.get());

  // Find the changed policies.
  scoped_ptr<base::DictionaryValue> changed_policies(
      new base::DictionaryValue());
  base::DictionaryValue::Iterator iter(*new_policies);
  while (!iter.IsAtEnd()) {
    base::Value* old_policy;
    if (!(old_policies_->Get(iter.key(), &old_policy) &&
          old_policy->Equals(&iter.value()))) {
      changed_policies->Set(iter.key(), iter.value().DeepCopy());
    }
    iter.Advance();
  }

  // Save the new policies.
  old_policies_.swap(new_policies);

  // Notify our client of the changed policies.
  if (!changed_policies->empty()) {
    policy_callback_.Run(changed_policies.Pass());
  }
}

}  // namespace policy_hack
}  // namespace remoting
