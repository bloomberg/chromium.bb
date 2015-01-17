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
#include "base/values.h"
#include "policy/policy_constants.h"
#include "remoting/host/dns_blackhole_checker.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

namespace remoting {
namespace policy_hack {

namespace {

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

    const base::Value* value = nullptr;

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
  if (from->GetString(policy::key::kRemoteAccessHostDebugOverridePolicies,
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

PolicyWatcher::PolicyWatcher(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      transient_policy_error_retry_counter_(0),
      old_policies_(new base::DictionaryValue()),
      default_values_(new base::DictionaryValue()),
      weak_factory_(this) {
  // Initialize the default values for each policy.
  default_values_->SetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                              true);
  default_values_->SetBoolean(policy::key::kRemoteAccessHostRequireTwoFactor,
                              false);
  default_values_->SetBoolean(policy::key::kRemoteAccessHostRequireCurtain,
                              false);
  default_values_->SetBoolean(policy::key::kRemoteAccessHostMatchUsername,
                              false);
  default_values_->SetString(policy::key::kRemoteAccessHostDomain,
                             std::string());
  default_values_->SetString(policy::key::kRemoteAccessHostTalkGadgetPrefix,
                             kDefaultHostTalkGadgetPrefix);
  default_values_->SetString(policy::key::kRemoteAccessHostTokenUrl,
                             std::string());
  default_values_->SetString(policy::key::kRemoteAccessHostTokenValidationUrl,
                             std::string());
  default_values_->SetString(
      policy::key::kRemoteAccessHostTokenValidationCertificateIssuer,
      std::string());
  default_values_->SetBoolean(policy::key::kRemoteAccessHostAllowClientPairing,
                              true);
  default_values_->SetBoolean(policy::key::kRemoteAccessHostAllowGnubbyAuth,
                              true);
  default_values_->SetBoolean(
      policy::key::kRemoteAccessHostAllowRelayedConnection, true);
  default_values_->SetString(policy::key::kRemoteAccessHostUdpPortRange, "");
#if !defined(NDEBUG)
  default_values_->SetString(
      policy::key::kRemoteAccessHostDebugOverridePolicies, std::string());
#endif

  // Initialize the fall-back values to use for unreadable policies.
  // For most policies these match the defaults.
  bad_type_values_.reset(default_values_->DeepCopy());
  bad_type_values_->SetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                               false);
  bad_type_values_->SetBoolean(
      policy::key::kRemoteAccessHostAllowRelayedConnection, false);
}

PolicyWatcher::~PolicyWatcher() {
}

void PolicyWatcher::StartWatching(
    const PolicyUpdatedCallback& policy_updated_callback,
    const PolicyErrorCallback& policy_error_callback) {
  if (!OnPolicyWatcherThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&PolicyWatcher::StartWatching,
                                      base::Unretained(this),
                                      policy_updated_callback,
                                      policy_error_callback));
    return;
  }

  policy_updated_callback_ = policy_updated_callback;
  policy_error_callback_ = policy_error_callback;
  StartWatchingInternal();
}

void PolicyWatcher::StopWatching(const base::Closure& stopped_callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&PolicyWatcher::StopWatchingOnPolicyWatcherThread,
                            base::Unretained(this)),
      stopped_callback);
}

void PolicyWatcher::StopWatchingOnPolicyWatcherThread() {
  StopWatchingInternal();
  weak_factory_.InvalidateWeakPtrs();
  policy_updated_callback_.Reset();
  policy_error_callback_.Reset();
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

  transient_policy_error_retry_counter_ = 0;

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
    policy_updated_callback_.Run(changed_policies.Pass());
  }
}

void PolicyWatcher::SignalPolicyError() {
  transient_policy_error_retry_counter_ = 0;
  policy_error_callback_.Run();
}

void PolicyWatcher::SignalTransientPolicyError() {
  const int kMaxRetryCount = 5;
  transient_policy_error_retry_counter_ += 1;
  if (transient_policy_error_retry_counter_ >= kMaxRetryCount) {
    SignalPolicyError();
  }
}

}  // namespace policy_hack
}  // namespace remoting
