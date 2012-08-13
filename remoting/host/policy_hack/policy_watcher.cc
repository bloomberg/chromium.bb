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
#include "base/time.h"
#include "base/values.h"
#include "remoting/host/constants.h"

namespace remoting {
namespace policy_hack {

namespace {
// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kFallbackReloadDelayMinutes = 15;

// Gets a boolean from a dictionary, or returns a default value if the boolean
// couldn't be read.
bool GetBooleanOrDefault(const base::DictionaryValue* dict, const char* key,
                         bool default_if_value_missing,
                         bool default_if_value_not_boolean) {
  if (!dict->HasKey(key)) {
    return default_if_value_missing;
  }
  const base::Value* value;
  if (dict->Get(key, &value) && value->IsType(base::Value::TYPE_BOOLEAN)) {
    bool bool_value;
    CHECK(value->GetAsBoolean(&bool_value));
    return bool_value;
  }
  return default_if_value_not_boolean;
}

// Gets a string from a dictionary, or returns a default value if the string
// couldn't be read.
std::string GetStringOrDefault(const base::DictionaryValue* dict,
                               const char* key,
                               const std::string& default_if_value_missing,
                               const std::string& default_if_value_not_string) {
  if (!dict->HasKey(key)) {
    return default_if_value_missing;
  }
  const base::Value* value;
  if (dict->Get(key, &value) && value->IsType(base::Value::TYPE_STRING)) {
    std::string string_value;
    CHECK(value->GetAsString(&string_value));
    return string_value;
  }
  return default_if_value_not_string;
}

// Copies a boolean from one dictionary to another, using a default value
// if the boolean couldn't be read from the first dictionary.
void CopyBooleanOrDefault(base::DictionaryValue* to,
                          const base::DictionaryValue* from, const char* key,
                          bool default_if_value_missing,
                          bool default_if_value_not_boolean) {
  to->Set(key, base::Value::CreateBooleanValue(
      GetBooleanOrDefault(from, key, default_if_value_missing,
                          default_if_value_not_boolean)));
}

// Copies a string from one dictionary to another, using a default value
// if the string couldn't be read from the first dictionary.
void CopyStringOrDefault(base::DictionaryValue* to,
                         const base::DictionaryValue* from, const char* key,
                         const std::string& default_if_value_missing,
                         const std::string& default_if_value_not_string) {
  to->Set(key, base::Value::CreateStringValue(
      GetStringOrDefault(from, key, default_if_value_missing,
                         default_if_value_not_string)));
}

// Copies all policy values from one dictionary to another, using default values
// when necessary.
scoped_ptr<base::DictionaryValue> AddDefaultValuesWhenNecessary(
    const base::DictionaryValue* from) {
  scoped_ptr<base::DictionaryValue> to(new base::DictionaryValue());
  CopyBooleanOrDefault(to.get(), from,
                       PolicyWatcher::kNatPolicyName, true, false);
  CopyBooleanOrDefault(to.get(), from,
                       PolicyWatcher::kHostRequireTwoFactorPolicyName,
                       false, false);
  CopyStringOrDefault(to.get(), from,
                      PolicyWatcher::kHostDomainPolicyName, "", "");
  CopyStringOrDefault(to.get(), from,
                      PolicyWatcher::kHostTalkGadgetPrefixPolicyName,
                      kDefaultTalkGadgetPrefix, kDefaultTalkGadgetPrefix);
  CopyBooleanOrDefault(to.get(), from,
                       PolicyWatcher::kHostRequireCurtainPolicyName,
                       false, false);

  return to.Pass();
}

}  // namespace

const char PolicyWatcher::kNatPolicyName[] =
    "RemoteAccessHostFirewallTraversal";

const char PolicyWatcher::kHostRequireTwoFactorPolicyName[] =
    "RemoteAccessHostRequireTwoFactor";

const char PolicyWatcher::kHostDomainPolicyName[] =
    "RemoteAccessHostDomain";

const char PolicyWatcher::kHostTalkGadgetPrefixPolicyName[] =
    "RemoteAccessHostTalkGadgetPrefix";

const char PolicyWatcher::kHostRequireCurtainPolicyName[] =
    "RemoteAccessHostRequireCurtain";

const char* const PolicyWatcher::kBooleanPolicyNames[] =
    { PolicyWatcher::kNatPolicyName,
      PolicyWatcher::kHostRequireTwoFactorPolicyName
    };

const int PolicyWatcher::kBooleanPolicyNamesNum =
    arraysize(kBooleanPolicyNames);

const char* const PolicyWatcher::kStringPolicyNames[] =
    { PolicyWatcher::kHostDomainPolicyName,
      PolicyWatcher::kHostTalkGadgetPrefixPolicyName
    };

const int PolicyWatcher::kStringPolicyNamesNum =
    arraysize(kStringPolicyNames);

PolicyWatcher::PolicyWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      old_policies_(new base::DictionaryValue()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
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

bool PolicyWatcher::OnPolicyWatcherThread() const {
  return task_runner_->BelongsToCurrentThread();
}

void PolicyWatcher::UpdatePolicies(
    const base::DictionaryValue* new_policies_raw) {
  DCHECK(OnPolicyWatcherThread());

  // Use default values for any missing policies.
  scoped_ptr<base::DictionaryValue> new_policies =
      AddDefaultValuesWhenNecessary(new_policies_raw);

  // Find the changed policies.
  scoped_ptr<base::DictionaryValue> changed_policies(
      new base::DictionaryValue());
  base::DictionaryValue::Iterator iter(*new_policies);
  while (iter.HasNext()) {
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
