// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_hack/nat_policy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

namespace {
// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kFallbackReloadDelayMinutes = 15;

}  // namespace

const char NatPolicy::kNatPolicyName[] = "RemoteAccessHostFirewallTraversal";

NatPolicy::NatPolicy(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      current_nat_enabled_state_(false),
      first_state_published_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

NatPolicy::~NatPolicy() {
}

void NatPolicy::StartWatching(const NatEnabledCallback& nat_enabled_cb) {
  if (!OnPolicyThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&NatPolicy::StartWatching,
                                      base::Unretained(this),
                                      nat_enabled_cb));
    return;
  }

  nat_enabled_cb_ = nat_enabled_cb;
  StartWatchingInternal();
}

void NatPolicy::StopWatching(base::WaitableEvent* done) {
  if (!OnPolicyThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&NatPolicy::StopWatching,
                                      base::Unretained(this), done));
    return;
  }

  StopWatchingInternal();
  weak_factory_.InvalidateWeakPtrs();
  nat_enabled_cb_.Reset();

  done->Signal();
}

void NatPolicy::ScheduleFallbackReloadTask() {
  DCHECK(OnPolicyThread());
  ScheduleReloadTask(
      base::TimeDelta::FromMinutes(kFallbackReloadDelayMinutes));
}

void NatPolicy::ScheduleReloadTask(const base::TimeDelta& delay) {
  DCHECK(OnPolicyThread());
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NatPolicy::Reload, weak_factory_.GetWeakPtr()),
      delay);
}

bool NatPolicy::OnPolicyThread() const {
  return task_runner_->BelongsToCurrentThread();
}

void NatPolicy::UpdateNatPolicy(base::DictionaryValue* new_policy) {
  DCHECK(OnPolicyThread());
  bool new_nat_enabled_state = false;
  if (!new_policy->HasKey(kNatPolicyName)) {
    // If unspecified, the default value of this policy is true.
    new_nat_enabled_state = true;
  } else {
    // Otherwise, try to parse the value and only change from false if we get
    // a successful read.
    base::Value* value;
    if (new_policy->Get(kNatPolicyName, &value) &&
        value->IsType(base::Value::TYPE_BOOLEAN)) {
      CHECK(value->GetAsBoolean(&new_nat_enabled_state));
    }
  }

  if (!first_state_published_ ||
      (new_nat_enabled_state != current_nat_enabled_state_)) {
    first_state_published_ = true;
    current_nat_enabled_state_ = new_nat_enabled_state;
    nat_enabled_cb_.Run(current_nat_enabled_state_);
  }
}

}  // namespace policy_hack
}  // namespace remoting
