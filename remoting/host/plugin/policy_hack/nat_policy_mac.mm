// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"

namespace remoting {
namespace policy_hack {

class NatPolicyMac : public NatPolicy {
 public:
  NatPolicyMac() {
  }

  virtual ~NatPolicyMac() {
  }

  virtual void StartWatching(const NatEnabledCallback& nat_enabled_cb)
      OVERRIDE {
    nat_enabled_cb_ = nat_enabled_cb;
    nat_enabled_cb_.Run(false);
  }

  virtual void StopWatching(base::WaitableEvent* done) OVERRIDE {
    nat_enabled_cb_.Reset();
    done->Signal();
  }

 private:
  NatEnabledCallback nat_enabled_cb_;
};

NatPolicy* NatPolicy::Create(base::MessageLoopProxy* message_loop_proxy) {
  return new NatPolicyMac();
}

}  // namespace policy_hack
}  // namespace remoting
