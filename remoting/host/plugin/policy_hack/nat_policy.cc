// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

namespace {
// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kFallbackReloadDelayMinutes = 15;

}  // namespace

const char NatPolicy::kNatPolicyName[] = "RemoteAccessHostFirewallTraversal";

NatPolicy::NatPolicy(base::MessageLoopProxy* message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy),
      current_nat_enabled_state_(false),
      first_state_published_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
}

NatPolicy::~NatPolicy() {
}

void NatPolicy::StartWatching(const NatEnabledCallback& nat_enabled_cb) {
  if (!OnPolicyThread()) {
    message_loop_proxy_->PostTask(FROM_HERE,
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
    message_loop_proxy_->PostTask(FROM_HERE,
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
  message_loop_proxy_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NatPolicy::Reload, weak_factory_.GetWeakPtr()),
      delay.InMilliseconds());
}

bool NatPolicy::OnPolicyThread() const {
  return message_loop_proxy_->BelongsToCurrentThread();
}

void NatPolicy::UpdateNatPolicy(base::DictionaryValue* new_policy) {
  DCHECK(OnPolicyThread());
  bool new_nat_enabled_state = false;
  if (!new_policy->HasKey(kNatPolicyName)) {
    // If unspecified, the default value of this policy is true.
    //
    // TODO(dmaclach): Currently defaults to false on Mac and Linux until we
    // have polices in place on those platforms.
#if defined(OS_WIN)
    new_nat_enabled_state = true;
#elif defined(OS_MACOSX)
    new_nat_enabled_state = false;
#elif defined(OS_LINUX)
    new_nat_enabled_state = false;
#else
    // Be conservative for now
    new_nat_enabled_state = false;
#endif  // defined(OS_WIN)
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
